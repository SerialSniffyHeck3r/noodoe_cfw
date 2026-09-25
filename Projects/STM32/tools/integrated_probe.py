"""Read-only live diagnostics immediately following a verified installation.

This does not halt/reset the CPU or access option bytes. The complete current
APP is checked against the installation before using its diagnostic addresses.
RAM snapshots remain live/non-atomic; DHCSR observations bracket the reads.
"""
import argparse,json,hashlib,struct,subprocess
from pathlib import Path
from bringup import require, run

FIELDS={
 'g_noodoe_runtime':'magic version started start_error io_heartbeat storage_heartbeat graphics_heartbeat io_ms storage_ms graphics_ms dash_result storage_result ambient_result clock_result bluetooth_result io_stack_free storage_stack_free heap_free heap_min obd_send_failures',
 'g_bsp_nor':'magic version ready result jedec_id status_register capacity_bytes spi_clock_hz reads bytes_read dma_completions dma_errors last_address last_length hal_status hal_error busy',
 'g_usb_bridge':'magic version initialized configured last_result resets suspends received_bytes transmitted_bytes rx_overflows tx_completions control_line_state',
 'g_storage_backup':'magic version initialized transport_verified result streaming sequence address remaining requests frames bytes_exported bad_requests',
 'g_bsp_bt_hci':'magic version opened baud tx_blocks rx_blocks tx_bytes rx_bytes errors last_hal_error tx_busy rx_busy reset_count cts_bypasses rx_complete tx_complete',
 'g_bsp_dash':'magic ready rx_bytes tx_bytes overflows errors restarts tx_enabled tx_busy last_hal_error',
 'g_dash_service':'magic version initialized enabled phase link_up error rx_bytes rx_frames link_frames tx_bytes tx_frames tx_requests tx_light tx_stops uart_errors checksum_errors overflows pending_replies reply_overflows light_index light_source light_sample_ms calibration_valid pending_id completed_id completed_result process_count',
 'g_dash_mailbox':'magic version request_seq command argument response_seq operation_id accept_result result completed_ms',
 'g_bsp_ambient':'magic ready error manufacturer device configuration raw millilux valid sample_ms samples failures',
 'g_bsp_clock':'magic ready error reads sets alarms backup19 year month day weekday hour minute second valid',
 'g_bluetooth':'magic version state last_error heartbeat manufacturer lmp_subversion hci_revision patch_bytes baud commands command_rejected hci_errors pairings key_generation key_persisted_generation stack_low_words',
 'g_graphics_performance':'magic version valid fps_tenths cpu_tenths target_fps_milli continuous window_ms window_frames frame_slots_missed cpu_window_cycles idle_window_cycles',
 'g_bsp_ram':'magic ready result geometry_bytes capacity_bytes tested_bytes first_bad_address expected observed allocated_bytes dma_verified',
 'g_bsp_power':'magic ready raw_ign_off ign_valid ign_on changes last_change_ms shutdown_outputs board_revision',
 'g_storage_service':'magic version initialized mounted last_result formats file_reads file_writes nvm_valid nvm_sequence nvm_length nvm_slot nvm_commits nvm_invalid',
 'g_noodoe_control':'magic version initialized connected link_generation process_count requests responses errors queue_full rx_bytes tx_bytes tx_failures parser_crc_errors parser_header_errors parser_timeouts last_opcode last_sequence last_result queued_replies phone_updates authorizations',
 'g_runtime_update':'ready result service_address service_bytes scratch_address scratch_bytes polls last_platform_result enabled_transaction commit_calls reset_calls state service_result connected authorized received_bytes verified_bytes',
 'g_settings':'magic version initialized state last_result storage_result provisioned record_generation pending_request completed_request completed_result writes write_failures key_generation',
}
CLI=Path('C:/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe')
# New records are separate symbols: old images retain their original prefix ABI.
FIELDS['g_bsp_bt_hci_fault']='magic version sequence reason tick '+ ' '.join('transport_'+v for v in FIELDS['g_bsp_bt_hci'].split())+' uart_sr uart_brr uart_cr1 uart_cr2 uart_cr3 gpioa_moder gpioa_idr gpioa_odr gpioa_afr0 gpioa_afr1 gpioi_moder gpioi_idr gpioi_odr '+' '.join('dma_'+d+'_'+v for d in ('rx','tx') for v in ('cr','ndtr','par','m0ar','m1ar','fcr'))
FIELDS['g_ambient_service']='magic version initialized state pending_id completed_id completed_result requested_hz desired_enabled retries next_retry_ms sample_age_ms stale '+' '.join('driver_'+v for v in (FIELDS['g_bsp_ambient']+' phase hal_status hal_error sr1 elapsed_ms bus_hz enabled').split())
FIELDS['g_bluetooth_control']='magic version request_sequence command ack_sequence accept_result operation_id phase completion_sequence completion_result controller_state result_sequence'
FIELDS['g_bsp_ambient_bitbang']='magic version bytes sequence operation_id request_seq result restore_result manufacturer device ids_valid elapsed_us phase failed_phase reg byte_index bit_index ack_count ack_mask initial_lines last_lines scl_high_checks scl_low_checks sda_high_checks sda_low_checks max_gap_us timing_uncertain clock_hz saved_cr1 final_cr1 saved_cr2 final_cr2 saved_ccr final_ccr saved_trise final_trise saved_fltr final_fltr saved_ph7 final_ph7 saved_pc9 final_pc9 pin_changes stop_result completed_ms max_low_us'
BITBANG_V2_FIELDS='first_pre_lines first_pre_us first_early_lines first_early_us first_late_lines first_late_us'
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--install',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 p.add_argument('--frequency-khz',type=int,choices=(50,100,400,950,4000),default=950);a=p.parse_args()
 summary=json.loads((a.install/'summary.json').read_text());manifest=json.loads((a.install/'image-manifest.json').read_text())
 if summary['result']!='pass':raise RuntimeError('A successful verified installation is required')
 image=(a.install/'app.bin').read_bytes()
 if hashlib.sha256(image).hexdigest()!=manifest['binary_sha256']:raise RuntimeError('Install image hash changed')
 require(0<len(image)<=0x70000 and len(image)==manifest['binary_size'] and
         int(manifest['flash_address'],0)==0x08010000,'Invalid APP bounds')
 a.output.mkdir(parents=True,exist_ok=False)
 base=[str(CLI),'-c','port=SWD','mode=HOTPLUG','freq='+str(a.frequency_khz),'sn='+summary['serial']]
 identity=a.output/'app-identity.bin'
 run(base+['-u','0x08010000',hex(len(image)),str(identity)],a.output/'app-identity.log',120)
 require(identity.read_bytes()==image,'Current APP differs from the installation; diagnostic addresses rejected')
 command=base+['-u','0xE000EDF0','4',str(a.output/'dhcsr-before.bin')]
 selected={}
 for name,fields in FIELDS.items():
  if name not in manifest['symbols']:continue
  if name=='g_bsp_dash' and manifest['symbols'][name]['size']==52:
   fields+=' tx_completed tx_failed tx_last_result'
  if name=='g_bsp_ambient_bitbang' and manifest['symbols'][name]['size'] in (208,212):
   fields+=' '+BITBANG_V2_FIELDS
   if manifest['symbols'][name]['size']==212:fields+=' pullup_mode'
  symbol=manifest['symbols'][name];length=len(fields.split())*4
  if length>symbol['size']:raise RuntimeError('Diagnostic layout does not fit '+name)
  address=int(symbol['address'],0) if isinstance(symbol['address'],str) else symbol['address']
  require(address%4==0 and 0x20000000<=address<=0x20030000-length,'Diagnostic outside main SRAM: '+name)
  selected[name]=fields.split();command+=['-u',hex(address),hex(length),str(a.output/(name+'.bin'))]
 command+=['-u','0xE000EDF0','4',str(a.output/'dhcsr-after.bin')]
 run(command,a.output/'probe.log',120)
 decoded={}
 for name,fields in selected.items():
  data=(a.output/(name+'.bin')).read_bytes()
  if len(data)!=len(fields)*4:raise RuntimeError('Incomplete '+name)
  decoded[name]=dict(zip(fields,struct.unpack('<'+'I'*len(fields),data)))
 before=struct.unpack('<I',(a.output/'dhcsr-before.bin').read_bytes())[0]
 after=struct.unpack('<I',(a.output/'dhcsr-after.bin').read_bytes())[0]
 decoded['_evidence']={'app_sha256':manifest['binary_sha256'],'current_app_verified':True,'installation':str(a.install),
                       'halted':bool((before|after)&(1<<17)),'dhcsr_before':before,'dhcsr_after':after,
                       'snapshot_atomic':False,'frequency_khz':a.frequency_khz}
 (a.output/'diagnostics.json').write_text(json.dumps(decoded,indent=2)+'\n')
 print(json.dumps(decoded,indent=2))
if __name__=='__main__':main()
