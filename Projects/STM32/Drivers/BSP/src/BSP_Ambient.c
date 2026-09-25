#include "BSP_Ambient.h"
#include "i2c.h"

/* TI OPT3001,7-bit address0x45. HAL expects one left shift. Sensor words are
 * MSB-first. MFi I2C1 and every power GPIO are outside this driver's ownership. */
#define ALS_ADDRESS (0x45U << 1U)
volatile BSP_Ambient_Diagnostics g_bsp_ambient;
static uint32_t last_poll;

/* Polling requires a ticking time base. Reject ISR/masked/unprivileged callers;
 * the service worker owns serialization, without locks across HAL waits. */
static uint32_t ContextAllowed(void)
{
    return !(__get_IPSR() || __get_PRIMASK() || __get_BASEPRI() ||
             __get_FAULTMASK() || (__get_CONTROL()&1U));
}

/* Capture HAL and SR1 before another operation can overwrite the evidence.
 * SR1 alone does not acknowledge ADDR. HAL status/error and hardware flags are
 * distinct observations, especially for ARLO during HAL's polling timeout. */
static uint32_t Finish(HAL_StatusTypeDef status,uint32_t start)
{
    g_bsp_ambient.sr1=hi2c3.Instance->SR1;
    g_bsp_ambient.hal_error=hi2c3.ErrorCode;
    g_bsp_ambient.hal_status=(uint32_t)status;
    g_bsp_ambient.elapsed_ms=HAL_GetTick()-start;
    g_bsp_ambient.error=status==HAL_OK?0U:(uint32_t)status+1U;
    if(status!=HAL_OK) {
        ++g_bsp_ambient.failures;
        g_bsp_ambient.ready=0U;g_bsp_ambient.valid=0U;
    }
    return g_bsp_ambient.error;
}

/* Failed reads cannot publish uninitialized data to the caller. */
static uint32_t ReadWord(uint8_t reg,uint16_t *value,BSP_Ambient_Phase phase)
{
    uint8_t data[2];
    uint32_t start=HAL_GetTick();g_bsp_ambient.phase=(uint32_t)phase;
    HAL_StatusTypeDef status=HAL_I2C_Mem_Read(&hi2c3,ALS_ADDRESS,reg,
        I2C_MEMADD_SIZE_8BIT,data,2U,BSP_AMBIENT_IO_TIMEOUT_MS);
    uint32_t result=Finish(status,start);
    if(!result)*value=((uint16_t)data[0]<<8U)|data[1];
    return result;
}

/* Continuous automatic range,800ms conversion. A failed write preserves the
 * last confirmed configuration/enabled values but invalidates ready/sample. */
uint32_t BSP_Ambient_SetEnabled(uint32_t enabled)
{
    if(enabled>1U)return 7U;
    if(!ContextAllowed())return 9U;
    if(!g_bsp_ambient.ready)return 8U;
    uint16_t config=enabled?0xCE10U:0xC810U;
    uint8_t bytes[2]={(uint8_t)(config>>8U),(uint8_t)config};
    uint32_t start=HAL_GetTick();g_bsp_ambient.phase=BSP_AMBIENT_PHASE_ENABLE;
    HAL_StatusTypeDef status=HAL_I2C_Mem_Write(&hi2c3,ALS_ADDRESS,1U,
        I2C_MEMADD_SIZE_8BIT,bytes,2U,BSP_AMBIENT_IO_TIMEOUT_MS);
    uint32_t result=Finish(status,start);g_bsp_ambient.valid=0U;
    if(!result) {
        g_bsp_ambient.configuration=config;g_bsp_ambient.enabled=enabled;
        last_poll=HAL_GetTick();
    }
    return result;
}

/* Reversible80/100/400kHz diagnostic. HAL MSP uses the generated PH7/PC9 AF pins;
 * no GPIO pull/pulse or power assumption is added. Calling status-returning HAL
 * avoids MX_I2C3_Init's fatal Error_Handler. All parameters match IOC except the
 * explicit diagnostic speed.80kHz avoids ES0206's88-100kHz repeated-START
 * limitation window; this does not assert that erratum caused observed ARLO.
 * Wrong sensor IDs stop before any sensor write. */
uint32_t BSP_Ambient_Probe(uint32_t bus_hz)
{
    if(bus_hz!=BSP_AMBIENT_SLOW_PROBE_HZ && bus_hz!=100000U &&
       bus_hz!=BSP_AMBIENT_DEFAULT_HZ)return 7U;
    if(!ContextAllowed())return 9U;
    g_bsp_ambient.magic=0x414C5331U;
    g_bsp_ambient.ready=0U;g_bsp_ambient.valid=0U;
    g_bsp_ambient.manufacturer=0U;g_bsp_ambient.device=0U;
    hi2c3.Instance=I2C3;
    uint32_t start=HAL_GetTick();g_bsp_ambient.phase=BSP_AMBIENT_PHASE_DEINIT;
    uint32_t result=Finish(HAL_I2C_DeInit(&hi2c3),start);
    if(result)return result;
    hi2c3.Init.ClockSpeed=bus_hz;
    hi2c3.Init.DutyCycle=I2C_DUTYCYCLE_2;
    hi2c3.Init.OwnAddress1=0U;
    hi2c3.Init.AddressingMode=I2C_ADDRESSINGMODE_7BIT;
    hi2c3.Init.DualAddressMode=I2C_DUALADDRESS_DISABLE;
    hi2c3.Init.OwnAddress2=0U;
    hi2c3.Init.GeneralCallMode=I2C_GENERALCALL_DISABLE;
    hi2c3.Init.NoStretchMode=I2C_NOSTRETCH_DISABLE;
    g_bsp_ambient.phase=BSP_AMBIENT_PHASE_INIT;start=HAL_GetTick();
    result=Finish(HAL_I2C_Init(&hi2c3),start);
    if(result)return result;
    g_bsp_ambient.bus_hz=bus_hz;
    g_bsp_ambient.phase=BSP_AMBIENT_PHASE_ANALOG_FILTER;start=HAL_GetTick();
    result=Finish(HAL_I2CEx_ConfigAnalogFilter(&hi2c3,I2C_ANALOGFILTER_ENABLE),start);
    if(result)return result;
    g_bsp_ambient.phase=BSP_AMBIENT_PHASE_DIGITAL_FILTER;start=HAL_GetTick();
    result=Finish(HAL_I2CEx_ConfigDigitalFilter(&hi2c3,0U),start);
    if(result)return result;
    uint16_t value;
    result=ReadWord(0x7EU,&value,BSP_AMBIENT_PHASE_MANUFACTURER);
    if(result)return result;
    g_bsp_ambient.manufacturer=value;
    result=ReadWord(0x7FU,&value,BSP_AMBIENT_PHASE_DEVICE);
    if(result)return result;
    g_bsp_ambient.device=value;
    if(g_bsp_ambient.manufacturer!=0x5449U || value!=0x3001U) {
        g_bsp_ambient.error=5U;return 5U;
    }
    /* Temporary write permission; failure clears ready again. Service snapshots
     * publish after this whole operation, so upper readers cannot see it. */
    g_bsp_ambient.ready=1U;
    return BSP_Ambient_SetEnabled(1U);
}

/* Legacy owner-only facade. A diagnostic probe never changes default policy. */
uint32_t BSP_Ambient_Init(void)
{
    return BSP_Ambient_Probe(BSP_AMBIENT_DEFAULT_HZ);
}

/* At most5Hz. No CRF means no new sample; retain the old sample/timestamp and
 * clear an old IO error on the successful config read. Service marks staleness.
 * Lux=0.01*mantissa*2^exponent, represented exactly in integer millilux. */
uint32_t BSP_Ambient_Poll(void)
{
    if(!ContextAllowed())return 9U;
    if(!g_bsp_ambient.ready)return 8U;
    if(!g_bsp_ambient.enabled)return 0U;
    uint32_t now=HAL_GetTick();
    if((uint32_t)(now-last_poll)<200U)return g_bsp_ambient.error;
    last_poll=now;
    uint16_t config,raw;
    uint32_t result=ReadWord(1U,&config,BSP_AMBIENT_PHASE_CONFIG);
    if(result)return result;
    g_bsp_ambient.configuration=config;
    if(!(config&0x80U))return 0U;
    result=ReadWord(0U,&raw,BSP_AMBIENT_PHASE_RESULT);
    if(result)return result;
    if((config&0x100U) || (raw>>12U)>11U) {
        g_bsp_ambient.error=6U;g_bsp_ambient.valid=0U;return 6U;
    }
    g_bsp_ambient.raw=raw;
    g_bsp_ambient.millilux=((uint32_t)(raw&0xFFFU)<<(raw>>12U))*10U;
    g_bsp_ambient.sample_ms=HAL_GetTick();++g_bsp_ambient.samples;
    g_bsp_ambient.valid=1U;return 0U;
}
