/* add your code here */
#include "AppleIntelWifiAdapterV2.hpp"

#include <IOKit/IOInterruptController.h>
#include <IOKit/IOCommandGate.h>

extern "C" {
    #include "dev-config.h"
}

#include "IwlTransOps.h"
#include "Mvm/IwlMvmOpMode.hpp"

#define super IO80211Controller

OSDefineMetaClassAndStructors(AppleIntelWifiAdapterV2, IO80211Controller)

static struct MediumTable
{
    IOMediumType type;
    UInt32 speed;
} mediumTable[] = {
    {kIOMediumIEEE80211None, 0},
    {kIOMediumIEEE80211Auto, 0}
};

void AppleIntelWifiAdapterV2::free() {
    IOLog("Driver free()");
//    releaseAll();
//    super::free();
}

bool AppleIntelWifiAdapterV2::init(OSDictionary *properties)
{
    IOLog("Driver init()");
    return super::init(properties);
}

IOService* AppleIntelWifiAdapterV2::probe(IOService *provider, SInt32 *score)
{
    IOLog("Driver Probe()");
    super::probe(provider, score);
    
    pciDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!pciDevice) {
        IOLog("Not pci device");
        return NULL;	
    }
    
    UInt16 vendorID = pciDevice->configRead16(kIOPCIConfigVendorID);
    UInt16 deviceID = pciDevice->configRead16(kIOPCIConfigDeviceID);
    UInt16 subSystemVendorID = pciDevice->configRead16(kIOPCIConfigSubSystemVendorID);
    UInt16 subSystemDeviceID = pciDevice->configRead16(kIOPCIConfigSubSystemID);
    UInt8 revision = pciDevice->configRead8(kIOPCIConfigRevisionID);
    IOLog("find pci device====>vendorID=0x%04x, deviceID=0x%04x, subSystemVendorID=0x%04x, subSystemDeviceID=0x%04x, revision=0x%02x", vendorID, deviceID, subSystemVendorID, subSystemDeviceID, revision);
    
    fConfiguration = getConfiguration(deviceID, subSystemDeviceID);
    if (!fConfiguration) {
        IOLog("ERROR: Failed to match configuration!");
        pciDevice = NULL;
        return NULL;
    }
    pciDevice->retain();
    
    IOLog("Successfully probed");
    return this;
}

int AppleIntelWifiAdapterV2::findMSIInterruptTypeIndex() {
    IOReturn ret;
    int index, source = 0;
    for (index = 0; ; index++)
    {
        int interruptType;
        ret = pciDevice->getInterruptType(index, &interruptType);
        if (ret != kIOReturnSuccess)
            break;
        if (interruptType & kIOInterruptTypePCIMessaged)
        {
            source = index;
            break;
        }
    }
    return source;
}

bool AppleIntelWifiAdapterV2::interruptFilter(OSObject* owner, IOFilterInterruptEventSource * src) {
    IOLog("interrupt filter");
    
    AppleIntelWifiAdapterV2* me = (AppleIntelWifiAdapterV2*)owner;

    if (me == 0) {
        TraceLog("Interrupt filter");
        return false;
    }

    /* Disable (but don't clear!) interrupts here to avoid
     * back-to-back ISRs and sporadic interrupts from our NIC.
     * If we have something to service, the tasklet will re-enable ints.
     * If we *don't* have something, we'll re-enable before leaving here.
     */
    
    //iwl_write32(me->fTrans, CSR_INT_MASK, 0x00000000);

    return true;
}

void AppleIntelWifiAdapterV2::interruptOccured(OSObject* owner, IOInterruptEventSource* sender, int count) {
    
    AppleIntelWifiAdapterV2* me = (AppleIntelWifiAdapterV2*)owner;
    
    if (me == 0) {
        return;
    }
    
    //me->iwl_pcie_irq_handler(0, me->fTrans);
}


bool AppleIntelWifiAdapterV2::start(IOService *provider)
{
    IOLog("Driver Start()");
    if (!super::start(provider)) {
        IOLog("failed to call super::start, releasing");
        releaseAll();
        return false;
    }
    
    int source = findMSIInterruptTypeIndex();
    fIrqLoop = IO80211WorkLoop::workLoop();
    fInterruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
                                                                            (IOInterruptEventAction) &AppleIntelWifiAdapterV2::interruptOccured,
                                                                                (IOFilterInterruptAction) &AppleIntelWifiAdapterV2::interruptFilter,
                                                                                pciDevice, source);
    
    
    if (!fInterruptSource) {
        IOLog("InterruptSource init failed!");
        releaseAll();
        return false;
    }
    
    if (fIrqLoop->addEventSource(fInterruptSource) != kIOReturnSuccess) {
        IOLog("EventSource registration failed");
        releaseAll();
        return false;
    }
    
    fInterruptSource->enable();
    gate = IOCommandGate::commandGate(this, (IOCommandGate::Action)&AppleIntelWifiAdapterV2::gateAction);
    
    if (fWorkLoop->addEventSource(gate) != kIOReturnSuccess) {
        IOLog("EventSource registration failed");
        releaseAll();
        return 0;
    }
    gate->enable();
    
    //fTrans = iwl_trans_pcie_alloc(fConfiguration);
    if (!fTrans) {
        IOLog("iwl_trans_pcie_alloc failed");
        releaseAll();
        return false;
    }
    fTrans->mbuf_cursor = IOMbufNaturalMemoryCursor::withSpecification(PAGE_SIZE, 1);
    fTrans->dev = this;
    fTrans->gate = gate;
    
        //fTrans->drv = iwl_drv_start(fTrans);
        
        if (!fTrans->drv) {
            TraceLog("DRV init failed!");
            releaseAll();
            return false;
        }

        
        /* if RTPM is in use, enable it in our device */
        if (fTrans->runtime_pm_mode != IWL_PLAT_PM_MODE_DISABLED) {
            /* We explicitly set the device to active here to
             * clear contingent errors.
             */
            PMinit();
            provider->joinPMtree(this);
            
            changePowerStateTo(kOffPowerState);// Set the public power state to the lowest level
            registerPowerDriver(this, gPowerStates, kNumPowerStates);
           // setIdleTimerPeriod(iwlwifi_mod_params.d0i3_timeout);
        }
        
        //transOps = new IwlTransOps(this);
        
    /*
        switch (fTrans->drv->fw.type) {
            case IWL_FW_DVM:
    //            opmode = new IwlDvmOpMode(transOps);
            case IWL_FW_MVM:
                opmode = new IwlMvmOpMode(transOps);
        }
        
        hw = opmode->start(fTrans, fTrans->cfg, &fTrans->drv->fw);
     */
    
    if (!createMediumDict()) {
        IOLog("failed to create medium dict, failing");
        releaseAll();
        return false;
    }
    
    if(!attachInterface((IONetworkInterface**)&netif)) {
        IOLog("failed to attach interface, failing");
        releaseAll();
        return false;
    }
    
    if(!netif) {
        IOLog("netif was null");
        releaseAll();
        return false;
    }
    netif->registerService();
    registerService();
    return true;
}

IOReturn AppleIntelWifiAdapterV2::gateAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3) {
    if (!owner) {
        return kIOReturnSuccess;
    }
    
    return kIOReturnSuccess;
}

void AppleIntelWifiAdapterV2::stop(IOService *provider)
{
    IOLog("Driver Stop()");
    
        if (fWorkLoop) {
            if (fInterruptSource) {
                fInterruptSource->disable();
                fWorkLoop->removeEventSource(fInterruptSource);
            }
        }
        

    //    opmode->stop(hw->priv);
       // iwl_drv_stop(fTrans->drv);
       // iwl_trans_pcie_free(fTrans);
        fTrans = NULL;
    
    if(netif) {
        detachInterface(netif, true);
        netif = NULL;
    }
    super::stop(provider);
    PMstop();
}

SInt32 AppleIntelWifiAdapterV2::apple80211Request(UInt32 request_type, int request_number, IO80211Interface* interface, void* data)
{
    if(request_type != SIOCGA80211 && request_type != SIOCSA80211)
    {
        IOLog("driver: got invalid request type");
        return kIOReturnError;
    }
    bool getting = (request_type == SIOCGA80211);
    IOLog("apple80211Request: %s(%d)", getting ? "get" : "set", request_number);
    return kIOReturnSuccess;
}

SInt32 AppleIntelWifiAdapterV2::apple80211VirtualRequest(uint,int,IO80211VirtualInterface *,void *)
{
    return kIOReturnSuccess;
}


IOReturn AppleIntelWifiAdapterV2::enable(IONetworkInterface *netif)
{
    IOLog("Driver Enable()");
    
    IOMediumType mediumType = kIOMediumIEEE80211Auto;
    IONetworkMedium *medium = IONetworkMedium::getMediumWithType(mediumDict, mediumType);
    setLinkStatus(kIONetworkLinkActive | kIONetworkLinkValid, medium);
    fTrans->intf = netif;
    
    return super::enable(netif);
}

IOReturn AppleIntelWifiAdapterV2::disable(IONetworkInterface *netif)
{
    IOLog("Driver Disable()");
    fTrans->intf = NULL;
    return super::disable(netif);
}


IOReturn AppleIntelWifiAdapterV2::getHardwareAddressForInterface(IO80211Interface *netif, IOEthernetAddress *addr)
{
    return getHardwareAddress(addr);
}

IOReturn AppleIntelWifiAdapterV2::setPromiscuousMode(bool active)
{
    return kIOReturnSuccess;
}

IOReturn AppleIntelWifiAdapterV2::setMulticastMode(bool active)
{
    return kIOReturnSuccess;
}

bool AppleIntelWifiAdapterV2::configureInterface(IONetworkInterface *netif)
{
    IOLog("Driver configureInterface()");
    if (!super::configureInterface(netif)) {
        IOLog("Failed super::configure");
        return false;
    }
    IONetworkData *nd = netif->getNetworkData(kIONetworkStatsKey);
    if (!nd || !(fNetworkStats = (IONetworkStats*)nd->getBuffer())) {
        return false;
    }
    
    nd = netif->getNetworkData(kIOEthernetStatsKey);
    if (!nd || !(fEthernetStats = (IOEthernetStats*)nd->getBuffer())) {
        return false;
    }
    return kIOReturnSuccess;
}

IO80211Interface* AppleIntelWifiAdapterV2::getNetworkInterface()
{
    return netif;
}

const OSString* AppleIntelWifiAdapterV2::newVendorString() const
{
    return OSString::withCString("Intel");
}

const OSString* AppleIntelWifiAdapterV2::newModelString() const
{
    return OSString::withCString(fConfiguration->name);
}

bool AppleIntelWifiAdapterV2::createWorkLoop() {
    if (!fWorkLoop) {
        fWorkLoop = IO80211WorkLoop::workLoop();
    }
    
    return (fWorkLoop != NULL);
}

IOWorkLoop* AppleIntelWifiAdapterV2::getWorkLoop() const {
    return fWorkLoop;
}

IOReturn AppleIntelWifiAdapterV2::getHardwareAddress(IOEthernetAddress *addrP) {
    UInt8 mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xF0, 0x0F};
    IOLog("Driver getHardwareAddress() - returning DE:AD:BE:EF:F0:0F");
   // memcpy(addrP->bytes, &mac, ETHER_ADDR_LEN);
    memcpy(addrP->bytes, &hw->wiphy->addresses[0], ETHER_ADDR_LEN);
    return kIOReturnSuccess;
}


int AppleIntelWifiAdapterV2::apple80211SkywalkRequest(uint,int, IO80211SkywalkInterface *,void *)
{
    return 0;
}

SInt32 AppleIntelWifiAdapterV2::disableVirtualInterface(IO80211VirtualInterface *)
{
    return 0;
}

SInt32 AppleIntelWifiAdapterV2::enableVirtualInterface(IO80211VirtualInterface *)
{
    return 0;
}

UInt32 AppleIntelWifiAdapterV2::getDataQueueDepth(OSObject *)
{
    return 1024;
}

SInt32 AppleIntelWifiAdapterV2::setVirtualHardwareAddress(IO80211VirtualInterface *, ether_addr *)
{
    return 0;
}

IO80211ScanManager* AppleIntelWifiAdapterV2::getPrimaryInterfaceScanManager(void)
{
    return NULL;
}

IO80211SkywalkInterface* AppleIntelWifiAdapterV2::getInfraInterface(void) {
    return NULL;
}

IO80211ControllerMonitor* AppleIntelWifiAdapterV2::getInterfaceMonitor(void) {
    return NULL;
}

void AppleIntelWifiAdapterV2::releaseAll() {
    RELEASE(pciDevice);
    RELEASE(fInterruptSource);
    RELEASE(fWorkLoop);
    RELEASE(mediumDict);
    
    RELEASE(fMemoryMap);
    /*
    if (fTrans) {
        iwl_trans_pcie_free(fTrans);
        fTrans = NULL;
    }
    */
    

}

bool AppleIntelWifiAdapterV2::createMediumDict() {
    UInt32 capacity = sizeof(mediumTable) / sizeof(struct MediumTable);
    
    mediumDict = OSDictionary::withCapacity(capacity);
    if (mediumDict == 0) {
        return false;
    }
    
    for (UInt32 i = 0; i < capacity; i++) {
        IONetworkMedium* medium = IONetworkMedium::medium(mediumTable[i].type, mediumTable[i].speed);
        if (medium) {
            IONetworkMedium::addMedium(mediumDict, medium);
            medium->release();
        }
    }
    
    if (!publishMediumDictionary(mediumDict)) {
        return false;
    }
    
    IONetworkMedium *m = IONetworkMedium::getMediumWithType(mediumDict, kIOMediumIEEE80211Auto);
    setSelectedMedium(m);
    return true;
}
