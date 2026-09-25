/* Real PhotoService/FatFs on the audited donor image, no device writes. */
#define TestMain LegacyTestMain
#define TestReload LegacyTestReload
#include "../stock_photos/test.c"
#undef TestMain
#undef TestReload
uint32_t TestMain(void)
{
    CHECK(StorageService_Init()==FR_OK);CHECK(StorageDisk_IsStock());
    for(uint32_t i=0;i<3;++i)CHECK(PhotoService_RequestLoad(i));
    for(uint32_t n=0;n<1000;++n)PhotoService_Process();
    CHECK(g_photos.ready_mask==7&&!g_photos.failed_mask&&!writes);
    for(uint32_t i=0;i<3;++i)CHECK(PhotoService_Get(i,&result_images[i]));
    uint32_t before_crc=Crc(result_images[0].pixels,result_images[0].bytes),before_alloc=allocated;
    PhotoService_Process();CHECK(g_photo_import.version==2&&g_photo_import.buffer);
    const uint8_t *source=(void*)0xA0000000U;memcpy((void*)g_photo_import.buffer,source,input_lengths[0]);
    g_photo_import.slot=PHOTO_IMPORT_WALLPAPER;g_photo_import.length=input_lengths[0];g_photo_import.crc32=0;
    g_photo_import.arm=0x42414B32U;g_photo_import.sequence=1;PhotoService_Process();
    CHECK(g_photo_import.ack==1&&g_photo_import.result==0x601&&!writes);
    g_photo_import.crc32=Crc(source,input_lengths[0]);g_photo_import.arm=0x42414B32U;g_photo_import.sequence=2;
    for(uint32_t n=0;n<1000&&g_photo_import.ack!=2;++n)PhotoService_Process();
    CHECK(g_photo_import.ack==2&&!g_photo_import.result&&writes);
    CHECK(g_photos.ready_mask==7&&Crc(result_images[0].pixels,result_images[0].bytes)==before_crc);
    CHECK(allocated==before_alloc); /* Decoder validates by discarding tiles. */
    uint32_t before=writes;
    CHECK(StorageService_CreateWallpaper(0,source,input_lengths[0],0x42414B32U)==FR_EXIST);CHECK(writes==before);
    char path[40];uint32_t length=0;
    CHECK(StorageService_FindWallpaper(0,path,sizeof(path),&length)==FR_OK);
    CHECK(!strcmp(path,"0:/WALL0.JPG")&&length==input_lengths[0]);
    CHECK(StorageService_FindAlbumPhoto(0,path,sizeof(path),&length)==FR_OK);CHECK(strncmp(path,"0:/album/0/",11)==0);
    return 0;
}
uint32_t TestReload(void)
{
    CHECK(LegacyTestReload()==0);
    char path[40];uint32_t length=0;
    CHECK(StorageService_FindWallpaper(0,path,sizeof(path),&length)==FR_OK&&!strcmp(path,"0:/WALL0.JPG"));
    return 0;
}
