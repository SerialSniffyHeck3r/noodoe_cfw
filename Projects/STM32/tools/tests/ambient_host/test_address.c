/* Reuse the independent START/edge/STOP electrical endpoint,not a scripted
 * production-phase response. The original ID diagnostic suite runs separately. */
#include "test_bitbang.c"

static uint32_t AddressRestored(void)
{
    return !g_bsp_ambient_address.restore_result &&
        g_bsp_ambient_address.saved_ph7==g_bsp_ambient_address.final_ph7 &&
        g_bsp_ambient_address.saved_pc9==g_bsp_ambient_address.final_pc9 &&
        I2C3->CR1==0x401U && I2C3->CCR==210U && hi2c3.Lock==HAL_UNLOCKED &&
        !(GPIOC->PUPDR&(3U<<18U)) && !(GPIOH->PUPDR&(3U<<14U)) &&
        !__get_PRIMASK() && max_masked<=2U;
}

uint32_t Ambient_AddressTestMain(void)
{
    Reset();sensor_mask=0U;needs_pullup=1U;
    CHECK(sizeof(g_bsp_ambient_address)==288U &&
          offsetof(BSP_AmbientAddress_Diagnostics,entry)==56U &&
          offsetof(BSP_AmbientAddress_Diagnostics,saved_cr1)==216U);
    /* Old cmd3 evidence must remain byte-identical throughout the new API. */
    memset((void *)&g_bsp_ambient_bitbang,0xA5,sizeof(g_bsp_ambient_bitbang));
    CHECK(BSP_AmbientAddress_ReadFixedCandidates(42U,91U)==BSP_ABB_NACK);
    CHECK(AddressRestored() && saw_pc9_pullup && g_bsp_ambient_address.sequence==2U);
    CHECK(g_bsp_ambient_address.magic==BSP_AMBIENT_ADDRESS_MAGIC &&
          g_bsp_ambient_address.version==1U && g_bsp_ambient_address.bytes==288U);
    CHECK(g_bsp_ambient_address.operation_id==42U && g_bsp_ambient_address.request_seq==91U);
    CHECK(g_bsp_ambient_address.attempted_mask==15U && !g_bsp_ambient_address.address_ack_mask &&
          !g_bsp_ambient_address.id_match_mask && wire_count==4U && starts==4U && stops==4U);
    for(uint32_t i=0U;i<sizeof(g_bsp_ambient_bitbang)/4U;++i)
        CHECK(((volatile uint32_t *)&g_bsp_ambient_bitbang)[i]==0xA5A5A5A5U);
    for(uint32_t i=0U;i<4U;++i) {
        CHECK(wire[i]==0x88U+i*2U && g_bsp_ambient_address.entry[i].address==0x44U+i);
        CHECK(g_bsp_ambient_address.entry[i].attempted && !g_bsp_ambient_address.entry[i].address_ack);
        CHECK(g_bsp_ambient_address.entry[i].result==BSP_ABB_NACK &&
              g_bsp_ambient_address.entry[i].phase==BSP_ABB_PHASE_ADDRESS_WRITE &&
              !g_bsp_ambient_address.entry[i].stop_result && !g_bsp_ambient_address.entry[i].ids_valid);
    }
    /* A genuine responder at each individual TI strap address must be read
     * at that same address. No register pointer is sent to NACKed addresses. */
    for(uint32_t i=0U;i<4U;++i) {
        Reset();sensor_mask=1U<<i;needs_pullup=1U;
        CHECK(BSP_AmbientAddress_ReadFixedCandidates(i+1U,0U)==BSP_ABB_OK);
        CHECK(AddressRestored() && g_bsp_ambient_address.address_ack_mask==(1U<<i) &&
              g_bsp_ambient_address.id_match_mask==(1U<<i) && wire_count==10U);
        CHECK(g_bsp_ambient_address.entry[i].ids_valid &&
              g_bsp_ambient_address.entry[i].manufacturer==0x5449U &&
              g_bsp_ambient_address.entry[i].device==0x3001U);
        CHECK(wire[i]==0x88U+i*2U && wire[i+1U]==wire[i] && wire[i+2U]==0x7EU &&
              wire[i+3U]==wire[i]+1U && wire[i+4U]==wire[i] && wire[i+5U]==0x7FU &&
              wire[i+6U]==wire[i]+1U);
    }
    Reset();sensor_mask=15U;needs_pullup=1U;
    CHECK(BSP_AmbientAddress_ReadFixedCandidates(1U,0U)==BSP_ABB_OK);
    CHECK(AddressRestored() && g_bsp_ambient_address.id_match_mask==15U && wire_count==28U);
    CHECK(!g_bsp_ambient_address.timing_uncertain && g_bsp_ambient_address.elapsed_us<100000U);
    /* Responding wrong IC identity stays visible and does not prevent another
     * allowed candidate from proving the expected manufacturer/device pair. */
    Reset();sensor_mask=3U;wrong_man_mask=1U;
    CHECK(BSP_AmbientAddress_ReadFixedCandidates(1U,0U)==BSP_ABB_OK);
    CHECK(AddressRestored() && g_bsp_ambient_address.address_ack_mask==3U &&
          g_bsp_ambient_address.id_match_mask==2U &&
          g_bsp_ambient_address.entry[0].result==BSP_ABB_ID_MISMATCH &&
          g_bsp_ambient_address.entry[0].ids_valid &&
          g_bsp_ambient_address.entry[0].manufacturer==0x1234U);
    Reset();sensor_mask=15U;wrong_dev_mask=15U;
    CHECK(BSP_AmbientAddress_ReadFixedCandidates(1U,0U)==BSP_ABB_ID_MISMATCH);
    CHECK(AddressRestored() && g_bsp_ambient_address.address_ack_mask==15U &&
          !g_bsp_ambient_address.id_match_mask && wire_count==28U);
    /* All six ID-read ACK positions may fail after the address-only ACK. A
     * STOP completes that candidate,then only the remaining fixed addresses. */
    for(uint32_t nack=2U;nack<=7U;++nack) {
        Reset();sensor_mask=1U;nak_at=nack;
        CHECK(BSP_AmbientAddress_ReadFixedCandidates(1U,0U)==BSP_ABB_NACK);
        CHECK(AddressRestored() && g_bsp_ambient_address.address_ack_mask==1U &&
              !g_bsp_ambient_address.entry[0].ids_valid && wire_count==nack+3U);
        CHECK(g_bsp_ambient_address.entry[0].result==BSP_ABB_NACK &&
              !g_bsp_ambient_address.entry[0].stop_result &&
              g_bsp_ambient_address.attempted_mask==15U);
    }
    /* A line failure in candidate1 aborts: rows2/3 are unvisited,not NACK. */
    Reset();sensor_mask=0U;clock_stretch=12U;
    CHECK(BSP_AmbientAddress_ReadFixedCandidates(1U,0U)==BSP_ABB_SCL_TIMEOUT);
    CHECK(AddressRestored() && g_bsp_ambient_address.attempted_mask==3U &&
          !g_bsp_ambient_address.entry[2].attempted && !g_bsp_ambient_address.entry[3].attempted);
    Reset();sensor_mask=15U;clock_stretch=106U;
    CHECK(BSP_AmbientAddress_ReadFixedCandidates(1U,0U)==BSP_ABB_SCL_TIMEOUT);
    CHECK(AddressRestored() && g_bsp_ambient_address.id_match_mask==1U &&
          g_bsp_ambient_address.attempted_mask==3U && g_bsp_ambient_address.entry[0].ids_valid);
    Reset();sensor_mask=15U;early_glitch=1U;
    CHECK(BSP_AmbientAddress_ReadFixedCandidates(1U,0U)==BSP_ABB_SDA_CONFLICT);
    CHECK(AddressRestored() && g_bsp_ambient_address.attempted_mask==1U && !wire_count);
    Reset();sensor_mask=0U;stop_failure=2U;
    CHECK(BSP_AmbientAddress_ReadFixedCandidates(1U,0U)==BSP_ABB_SDA_CONFLICT);
    CHECK(AddressRestored() && g_bsp_ambient_address.attempted_mask==3U &&
          g_bsp_ambient_address.entry[1].phase==BSP_ABB_PHASE_STOP &&
          g_bsp_ambient_address.entry[1].stop_result==BSP_ABB_SDA_CONFLICT);
    Reset();sensor_mask=15U;gap_at=12U;gap_cycles=168000U*35U;
    CHECK(BSP_AmbientAddress_ReadFixedCandidates(1U,0U)==BSP_ABB_TIMING_UNCERTAIN);
    CHECK(AddressRestored() && g_bsp_ambient_address.timing_uncertain &&
          g_bsp_ambient_address.max_low_us>=35000U && g_bsp_ambient_address.attempted_mask==1U);
    Reset();sensor_mask=15U;force_restore=1U;
    CHECK(BSP_AmbientAddress_ReadFixedCandidates(1U,0U)==BSP_ABB_RESTORE_FAILED);
    CHECK(g_bsp_ambient_address.restore_result==1U && hi2c3.Lock==HAL_LOCKED &&
          !(I2C3->CR1&1U) && I2C3->CCR==211U && !(GPIOC->PUPDR&(3U<<18U)));
    Reset();sensor_mask=15U;foreign_pull=1U;
    CHECK(BSP_AmbientAddress_ReadFixedCandidates(1U,0U)==BSP_ABB_RESTORE_FAILED);
    CHECK(g_bsp_ambient_address.restore_result==1U && hi2c3.Lock==HAL_LOCKED &&
          !(GPIOC->PUPDR&(3U<<18U)) && !__get_PRIMASK());
    /* Precheck and reentrant rejections never clock an unapproved transaction. */
    Reset();I2C3->CR2|=I2C_CR2_DMAEN;
    CHECK(BSP_AmbientAddress_ReadFixedCandidates(1U,2U)==BSP_ABB_PRECONDITION);
    CHECK(!g_bsp_ambient_address.attempted_mask && !g_bsp_ambient_address.pin_changes &&
          g_bsp_ambient_address.restore_result==2U && !wire_count);
    CHECK(g_bsp_ambient_address.entry[0].address==0x44U &&
          g_bsp_ambient_address.entry[3].address==0x47U);
    Reset();sensor_mask=0U;reenter=1U;
    CHECK(BSP_AmbientAddress_ReadFixedCandidates(9U,3U)==BSP_ABB_NACK);
    CHECK(AddressRestored() && reentry_result==BSP_ABB_PRECONDITION &&
          g_bsp_ambient_address.operation_id==9U && !g_bsp_ambient_bitbang.sequence);
    CHECK((GPIOC->PUPDR&(3U<<2U))==(2U<<2U) && (GPIOH->PUPDR&(3U<<4U))==(1U<<4U));
    /* A subsequent ordinary0x45 diagnostic uses its original public record
     * and NOPULL policy; the command4 result remains separately available. */
    wire_count=starts=stops=wire_clocks=0U;masked_writes=max_masked=0U;
    sensor_mask=2U;saw_pc9_pullup=0U;
    CHECK(BSP_AmbientBitbang_ReadIDs(10U,0U)==BSP_ABB_OK);
    CHECK(Restored() && !saw_pc9_pullup && !g_bsp_ambient_bitbang.pullup_mode &&
          g_bsp_ambient_bitbang.operation_id==10U && g_bsp_ambient_address.operation_id==9U);
    CHECK(!g_mock_error);return 0U;
}
