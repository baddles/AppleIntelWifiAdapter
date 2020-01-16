/* add your code here */
#ifndef AppleIntelWifiAdapter_hpp
#define AppleIntelWifiAdapter_hpp

#include "apple80211/IO80211Controller.h"
#include "apple80211/IO80211Interface.h"
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/network/IOEthernetController.h>

#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOLib.h>



#define    RELEASE(x)    if(x){(x)->release();(x)=NULL;}

extern "C" {
    #include "dev-config.h"
    #include "iwl-debug.h"
    
#include "iwlwifi/iwl-csr.h"
#include "iwl-drv.h"
#include "iwl-trans.h"
#include "iwl-fh.h"
#include "iwl-prph.h"
#include "iwl-config.h"
#include "iwl-eeprom-parse.h"
#include "iwlwifi/pcie/internal.h"
#include "iwlwifi/iwl-scd.h"
#include <linux/jiffies.h>
}


#include "iwlwifi/dma-utils.h"
#include "iw_utils/allocation.h"
#include "TransOps.h"
#include "IwlOpModeOps.h"
#include <IOKit/network/IOMbufMemoryCursor.h>
#include <IOKit/IOMemoryCursor.h>

#define CONFIG_IWLMVM // Need NVM mode at least to see that code is compiling
#define CONFIG_IWLWIFI_PCIE_RTPM // Powermanb

enum {
    kOffPowerState,
    kOnPowerState,
    kNumPowerStates
};

static IOPMPowerState gPowerStates[kNumPowerStates] = {
    // kOffPowerState
    {kIOPMPowerStateVersion1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    // kOnPowerState
    {kIOPMPowerStateVersion1, (kIOPMPowerOn | kIOPMDeviceUsable), kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}
};

class AppleIntelWifiAdapterV2 : public IO80211Controller
{
    OSDeclareDefaultStructors( AppleIntelWifiAdapterV2 )
public:
    
    bool init(OSDictionary *properties) override;
    void free() override;
    IOService* probe(IOService* provider, SInt32* score) override;
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    SInt32 apple80211Request(unsigned int, int, IO80211Interface*, void*) override;
    SInt32 apple80211VirtualRequest(uint,int,IO80211VirtualInterface *,void *) override;
    const OSString* newVendorString() const override;
    const OSString* newModelString() const override;
    IOReturn getHardwareAddressForInterface(IO80211Interface* netif, IOEthernetAddress* addr) override;
    IOReturn getHardwareAddress(IOEthernetAddress* addrP) override;
    IOReturn enable(IONetworkInterface *netif) override;
    IOReturn disable(IONetworkInterface *netif) override;
    bool configureInterface(IONetworkInterface *netif) override;
    IO80211Interface *getNetworkInterface() override;
    IOReturn setPromiscuousMode(bool active) override;
    IOReturn setMulticastMode(bool active) override;
    SInt32 monitorModeSetEnabled(IO80211Interface*, bool, unsigned int) {
        return kIOReturnSuccess;
    }
    bool createWorkLoop() override;
    IOWorkLoop* getWorkLoop() const override;
    int apple80211SkywalkRequest(uint,int, IO80211SkywalkInterface *,void *) override;
    SInt32 disableVirtualInterface(IO80211VirtualInterface *) override;
    SInt32 enableVirtualInterface(IO80211VirtualInterface *) override;
    UInt32 getDataQueueDepth(OSObject *) override;
    SInt32 setVirtualHardwareAddress(IO80211VirtualInterface *,ether_addr *) override;
    IO80211ScanManager* getPrimaryInterfaceScanManager(void) override;
    IO80211SkywalkInterface* getInfraInterface(void) override;
    IO80211ControllerMonitor* getInterfaceMonitor(void) override;
    
    
public:
    /*
    int iwl_trans_pcie_start_fw(struct iwl_trans *trans, const struct fw_img *fw, bool run_in_rfkill); // line 1224
    void iwl_trans_pcie_stop_device(struct iwl_trans *trans, bool low_power); // line 1347
    int iwl_trans_pcie_start_hw(struct iwl_trans *trans, bool low_power); // line 1675
    void iwl_trans_pcie_op_mode_leave(struct iwl_trans *trans); // line 1687
    IwlOpModeOps *opmode;
     */
private:
    bool createMediumDict();
    inline void releaseAll();
    
    static void interruptOccured(OSObject* owner, IOInterruptEventSource* sender, int count);
    static bool interruptFilter(OSObject* owner, IOFilterInterruptEventSource * src);
    static IOReturn gateAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
    
    int findMSIInterruptTypeIndex();
    
    
    /*
    void iwl_pcie_set_pwr(struct iwl_trans *trans, bool vaux); // line 186
    void iwl_pcie_apm_config(struct iwl_trans *trans); // line 204
    int iwl_pcie_apm_init(struct iwl_trans *trans); // line 232
    void iwl_pcie_apm_stop(struct iwl_trans *trans, bool op_mode_leave); // line 460
    int iwl_pcie_nic_init(struct iwl_trans *trans); // line 505
    
    void iwl_pcie_init_msix(struct iwl_trans_pcie *trans_pcie); // line 1115
    
    void _iwl_trans_pcie_stop_device(struct iwl_trans *trans, bool low_power); // line 1130
    
    
    
    void iwl_trans_pcie_handle_stop_rfkill(struct iwl_trans *trans, bool was_in_rfkill); // line 1318
    
    void iwl_pcie_set_interrupt_capa(struct iwl_trans *trans); // line 1489
    int _iwl_trans_pcie_start_hw(struct iwl_trans *trans, bool low_power); // line 1636

    
    
    void iwl_trans_pcie_free(struct iwl_trans* trans); // line 1776
    
    struct iwl_trans* iwl_trans_pcie_alloc(const struct iwl_cfg *cfg); // line 2988
    
    // trans-gen2.c
    int iwl_pcie_gen2_apm_init(struct iwl_trans *trans);
    void iwl_pcie_gen2_apm_stop(struct iwl_trans *trans, bool op_mode_leave);
    void _iwl_trans_pcie_gen2_stop_device(struct iwl_trans *trans, bool low_power);
    void iwl_trans_pcie_gen2_stop_device(struct iwl_trans *trans, bool low_power);
    int iwl_pcie_gen2_nic_init(struct iwl_trans *trans);
    void iwl_trans_pcie_gen2_fw_alive(struct iwl_trans *trans, u32 scd_addr);
    int iwl_trans_pcie_gen2_start_fw(struct iwl_trans *trans, const struct fw_img *fw,
                                     bool run_in_rfkill);

    
    // rx.c
    void iwl_pcie_irq_handler(int irq, void *dev_id);
    
    void iwl_pcie_handle_rfkill_irq(struct iwl_trans *trans);
    void iwl_pcie_irq_handle_error(struct iwl_trans *trans);

    void iwl_pcie_rx_handle(struct iwl_trans *trans, int queue);
    void iwl_pcie_rx_handle_rb(struct iwl_trans *trans, struct iwl_rxq *rxq,
                               struct iwl_rx_mem_buffer *rxb, bool emergency);
    
    // tx.c
    int iwl_pcie_txq_alloc(struct iwl_trans *trans, struct iwl_txq *txq, int slots_num, bool cmd_queue); // line 487
    int iwl_pcie_txq_init(struct iwl_trans *trans, struct iwl_txq *txq, int slots_num, bool cmd_queue); // line 551
    int iwl_pcie_tx_alloc(struct iwl_trans *trans); // line 907
    int iwl_pcie_tx_init(struct iwl_trans *trans); // line 973
    void iwl_pcie_txq_progress(struct iwl_txq *txq); // line 1034
    void iwl_pcie_cmdq_reclaim(struct iwl_trans *trans, int txq_id, int idx); // line 1211

    void iwl_pcie_hcmd_complete(struct iwl_trans *trans, struct iwl_rx_cmd_buffer *rxb); // line 1723
    int iwl_trans_pcie_tx(struct iwl_trans *trans, struct sk_buff *skb,
                          struct iwl_device_cmd *dev_cmd, int txq_id); // line 2256
    
     */
    const struct iwl_cfg* fConfiguration;
    IOPCIDevice *pciDevice;
    UInt16 fDeviceId;
    UInt16 fSubsystemId;
    IOCommandGate *gate;
    
    IO80211Interface *netif;
    IOWorkLoop *fWorkLoop;
    IOWorkLoop *fIrqLoop;
    OSDictionary *mediumDict;
    
    IONetworkStats *fNetworkStats;
    IOEthernetStats *fEthernetStats;
    IOFilterInterruptEventSource* fInterruptSource;
    
    IOMemoryMap *fMemoryMap;
    struct ieee80211_hw *hw;
    struct iwl_nvm_data *fNvmData;
    struct iwl_trans* fTrans;
    TransOps *transOps;
};

#endif
