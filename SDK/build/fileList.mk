




























c_SRC_FILES += audio/framework/plugs/source/a2dp_file.c audio/framework/plugs/source/a2dp_streamctrl.c audio/framework/plugs/source/esco_file.c audio/framework/plugs/source/adc_file.c audio/framework/nodes/esco_tx_node.c audio/framework/nodes/plc_node.c audio/framework/nodes/volume_node.c audio/framework/node_list.c
c_SRC_FILES += audio/framework/nodes/cvp_sms_node.c



c_SRC_FILES += audio/framework/nodes/cvp_dms_node.c
c_SRC_FILES += audio/common/audio_node_config.c audio/common/audio_dvol.c audio/common/audio_general.c audio/common/audio_build_needed.c audio/common/audio_plc.c audio/common/audio_noise_gate.c audio/common/audio_ns.c audio/common/audio_utils.c audio/common/amplitude_statistic.c audio/common/frame_length_adaptive.c audio/common/bt_audio_energy_detection.c audio/common/audio_event_handler.c audio/common/debug/audio_debug.c audio/common/power/mic_power_manager.c audio/common/audio_volume_mixer.c audio/common/audio_effect_verify.c audio/common/pcm_data/sine_pcm.c
c_SRC_FILES += audio/common/demo/audio_demo.c
c_SRC_FILES += audio/common/demo/hw_math_v2_demo.c
c_SRC_FILES += audio/interface/player/tone_player.c audio/interface/player/ring_player.c audio/interface/player/a2dp_player.c audio/interface/player/esco_player.c audio/interface/player/key_tone_player.c audio/interface/player/dev_flow_player.c audio/interface/player/adda_loop_player.c
c_SRC_FILES += audio/interface/recoder/esco_recoder.c audio/interface/recoder/dev_flow_recoder.c
c_SRC_FILES += audio/interface/user_defined/audio_dsp_low_latency_player.c audio/interface/user_defined/env_noise_recoder.c
c_SRC_FILES += audio/effect/eq_config.c audio/effect/audio_dc_offset_remove.c audio/effect/effects_adj.c audio/effect/effects_dev.c audio/effect/effects_default_param.c audio/effect/node_param_update.c
c_SRC_FILES += audio/CVP/audio_aec.c audio/CVP/audio_cvp.c audio/CVP/audio_cvp_dms.c audio/CVP/audio_cvp_online.c audio/CVP/audio_cvp_ref_task.c audio/CVP/audio_cvp_config.c
c_SRC_FILES += audio/interface/player/tws_tone_player.c
c_SRC_FILES += audio/framework/plugs/source/linein_file.c
c_SRC_FILES += audio/cpu/common.c
c_SRC_FILES += apps/common/lib_version/version_check.c apps/common/config/bt_profile_config.c



c_SRC_FILES += apps/common/lib_log_config/btctrler_log_config.c apps/common/lib_log_config/btstack_log_config.c apps/common/lib_log_config/driver_log_config.c apps/common/lib_log_config/media_log_config.c apps/common/lib_log_config/net_log_config.c apps/common/lib_log_config/system_log_config.c apps/common/lib_log_config/update_log_config.c
c_SRC_FILES += apps/common/debug/debug_uart_config.c
c_SRC_FILES += apps/common/fat_nor/cfg_private.c




c_SRC_FILES += apps/common/debug/memory_debug.c
c_SRC_FILES += apps/common/update/update.c



c_SRC_FILES += apps/common/update/testbox_update.c




c_SRC_FILES += apps/common/update/testbox_uart_update.c
c_SRC_FILES += apps/common/ui/pwm_led/led_ui_api.c apps/common/ui/pwm_led/led_ui_tws_sync.c
c_SRC_FILES += apps/common/jldtp/uart_transport.c apps/common/jldtp/jldtp_manager.c
c_SRC_FILES += apps/common/device/key/key_driver.c



c_SRC_FILES += apps/common/device/key/iokey.c
c_SRC_FILES += apps/common/device/usb/device/usb_pll_trim.c
c_SRC_FILES += apps/common/device/usb/device/msd_upgrade.c
c_SRC_FILES += cpu/components/iic_soft.c cpu/components/iic_api.c cpu/components/ir_encoder.c cpu/components/ir_decoder.c cpu/components/led_spi.c cpu/components/rdec_soft.c
c_SRC_FILES += cpu/components/led_api.c cpu/components/two_io_led.c
c_SRC_FILES += cpu/config/gpio_file_parse.c cpu/config/lib_power_config.c
c_SRC_FILES += audio/cpu/br56/audio_setup.c audio/cpu/br56/audio_mic_capless.c audio/cpu/br56/audio_config.c audio/cpu/br56/audio_configs_dump.c





c_SRC_FILES += audio/cpu/br56/audio_dai/audio_pdm.c



c_SRC_FILES += audio/cpu/br56/audio_anc_platform.c





c_SRC_FILES += audio/cpu/br56/audio_accelerator/hw_fft.c



c_SRC_FILES += audio/cpu/br56/audio_demo/audio_adc_demo.c




c_SRC_FILES += audio/cpu/br56/audio_demo/audio_dac_demo.c audio/cpu/br56/audio_demo/audio_fft_demo.c #audio/cpu/br56/audio_demo/audio_alink_demo.c #audio/cpu/br56/audio_demo/audio_pdm_demo.c
c_SRC_FILES += cpu/br56/setup.c cpu/br56/overlay_code.c cpu/br56/rf_api.c




c_SRC_FILES += cpu/br56/charge/charge_ocp.c



c_SRC_FILES += cpu/br56/charge/charge.c cpu/br56/charge/charge_config.c





c_SRC_FILES += cpu/br56/charge/chargestore.c cpu/br56/charge/chargestore_config.c
c_SRC_FILES += cpu/components/pwm_led_v2.c



c_SRC_FILES += cpu/br56/power/power_port.c cpu/br56/power/power_gate.c cpu/br56/power/key_wakeup.c cpu/br56/power/power_app.c cpu/br56/power/power_config.c






c_SRC_FILES += cpu/br56/ui_driver/led7/led7_driver.c cpu/br56/ui_driver/ui_common.c
c_SRC_FILES += apps/earphone/demo/pbg_demo.c
c_SRC_FILES += apps/earphone/mode/bt/dual_phone_call.c apps/earphone/mode/bt/dual_a2dp_play.c
c_SRC_FILES += apps/earphone/tools/app_testbox.c
c_SRC_FILES += apps/earphone/ui/led/led_config.c apps/earphone/ui/led/led_ui_msg_handler.c
