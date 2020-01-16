//
//  config.h
//  AppleIntelWifiAdapter
//
//  Created by Harrison Ford on 1/15/20.
//  Copyright © 2020 钟先耀. All rights reserved.
//

#ifndef config_h
#define config_h

#include <linux/types.h>

#include "iwl-config.h"

struct iwl_cfg* getConfiguration(u16 deviceId, u16 subSystemId);

#endif /* config_h */
    
