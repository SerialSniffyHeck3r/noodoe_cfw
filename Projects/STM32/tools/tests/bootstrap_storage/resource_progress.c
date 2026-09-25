#include "Bootstrap_Resources.h"
#include "InstallSession.h"
static BootstrapStorage storage;
static InstallSession install;
uint32_t ResourceProgress(void)
{
 uint32_t out[20];
 storage.bytes=524288U;storage.resource_total=102200U;
 InstallSession_Begin(&install,0,1);
 for(uint32_t phase=0;phase<5;phase++){
  storage.state=BS_WRITING;storage.phase=phase;
  uint32_t total=BootstrapResources_ProgressTotal(&storage);
  if(total!=(phase==0?0x9000U:524288U))return 1;
  InstallSession_Observe(&install,20,INSTALL_TRANSFER,BS_WRITING,0,9,total,total,0,0,1);
  InstallSession_Snapshot(&install,out);
  if(out[10]>out[11]||out[13]>out[14])return 2;
 }
 storage.state=BS_RESEED_CHECK;
 if(BootstrapResources_ProgressTotal(&storage)!=1048576U)return 3;
 storage.state=BS_AUDITING;storage.phase=3;
 if(BootstrapResources_ProgressTotal(&storage)!=102200U)return 4;
 return 0;
}
