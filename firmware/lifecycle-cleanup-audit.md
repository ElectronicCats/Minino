# Minino firmware: app exit and cleanup audit

- **Date:** 2026-11-26
- **Branch:** `feat/ble-saturate-and-sdr` (audit written against HEAD `b1654b24`; fixes since committed up to `c0f2b880`)
- **ESP-IDF checked against:** v5.5.1
- **Scope:** application exit and termination paths. The audit below changed no code; the fixes it drove are tracked per phase in section 5.
- **Status (latest):** S1–S6 done (committed, hardware-verified 2026-11-27 — section 10). A18 done (committed; #1, #3, #4 hardware-verified, #2 stays latent — section 11). A16 fixed and hardware-verified 2026-11-27 (uncommitted, section 12); joining a real AP for it timed out (both the user's hotspot and a saved network — likely a 2.4/5 GHz band mismatch, not an A16 issue), so the thread-lifecycle fix was verified against a deterministic non-responding target instead. Open: the Modbus TCP/cat_dos `radio_lease` conversion (reverted) and the rest of S3. `error_handler` wiring and a real task watchdog left as deliberate non-work (section 3). The only committed work still untested on hardware is what needs equipment or an environment (A7/Modbus+cat_dos with a real AP, the RF-transmit attack paths, OTA upload, GPS-fix CSV content, logs-to-UART — see 7.4 / 8.3 / 9.2 / 10.3).

The biggest problem is not inside any single cleanup routine. Most apps' cleanup never runs when the user leaves with the back button. On top of that, the Wi-Fi driver's state is tracked in three places that drift apart, so nothing puts Wi-Fi back to its boot state.

The audit traced the lifecycle layer (`app_manager`, `menus_module`, `radio_lease`, `wifi_service`, `wifi_controller`, `task_manager`), every app reachable from the menu table, and the components they drive. Where behaviour depends on ESP-IDF, it was checked against the IDF 5.5.1 source tree. Nothing was run on hardware.

- **Confirmed** means the failing path can be followed through the code.
- **Plausible** means it depends on timing, or on closed-source driver behaviour that could not be fully verified.

---

## 1. Problems that span many apps

- [x] **S1. For ten apps, the back button does nothing, so their cleanup never runs.** Confirmed.
- **Where:** [app_manager.c:25](../firmware/main/modules/app_manager/app_manager.c#L25). `app_manager_exit()` returns immediately when no app is active.
- **Cause:** all `app_manager_enter_wrapper_*` functions except Modbus TCP's have zero callers. The menu table enters these apps through their old entry functions, so `app_manager` never becomes active. When their back handler calls `app_manager_exit()`, nothing happens: `on_exit` doesn't run and the UI freezes. This is the same bug commit 2f8510c9 fixed for Modbus TCP only.
- **Affected apps:** Wi-Fi analyzer, captive portal, deauth detector, Drone ID, DroneID scanner, SSID spam, cat_dos, I2C scanner, OTA, file manager. HID returns input but never redraws the menu.
- **Current escape:** only the console `exit` command (which runs the menu's `on_exit_cb`) or a reboot.
- **Fix:** add one fallback in the shared function: if `s_active == NULL`, call `menus_module_exit_app()`. That covers every caller at once.
- **Warning:** fixing S1 makes several `on_exit` routines run for the first time, and some of them crash (A1, A2, A8, A18). Fix those together with S1.
- **Hardware test (section 6):** 269fe1c2 did not fix HID. Its back handler cleared `in_app` before calling `app_manager_exit()`, so the fallback skipped the redraw. Flock had the same blank screen. Both are fixed in Phase 7.

- [x] **S2. Three separate records of the Wi-Fi driver's state drift apart.** Confirmed.
- **The three records:** the `radio_lease` reference counts, `wifi_service`'s `s_state.is_up`, and `wifi_controller`'s `wifi_driver_initialized` flag.
- **Cause:** captive, SSID spam, Zigbee (×3), the mesh mapper, OpenThread (×2), warbee and warthread call `wifi_service_deinit()` directly. That leaves `wifi_driver_initialized = true`.
- **Effect:** the next `wifi_driver_init_sta()` or `wifi_driver_init_null()` ([wifi_controller.c:107](../firmware/components/wifi_controller/wifi_controller.c#L107), [:156](../firmware/components/wifi_controller/wifi_controller.c#L156)) takes the "already initialized" branch:
  - If the service is down, `wifi_service_set_mode()` returns `INVALID_STATE`, so `radio_lease_wifi_acquire()` fails (Flock, rf_spectrum).
  - If an app's exit already re-ran `wifi_service_init(STA)`, it returns true without ever calling `esp_wifi_start()`, so promiscuous mode runs on a stopped driver and receives no frames.
- **Example:** Analyzer, then Zigbee sniffer, then Flock or rf_spectrum. The last one sees nothing.
- **Fix:** in `wifi_controller`, test `wifi_driver_initialized && wifi_service_is_up_get()` (and whether the driver is started) instead of trusting the cached flag.
- **Minor:** `s_current_channel` is never reset on deinit, so `wifi_driver_get_channel()` can report a stale channel.

- [x] **S3. Many apps bring Wi-Fi up or down without going through `radio_lease`.** Confirmed. _Parcial: convertidos a `radio_lease_wifi_acquire/release` el analyzer (NULL), captive, SSID spam, el detector de deauth, el `wifi_scanner` compartido y deauth (claim APSTA por sesión, H7); quedan Modbus TCP + cat_dos (`wifi_ap_manager`: la conversión `c51229bf` se revirtió en `825f8f18`), wardriving/A13, rf_spectrum/A11, DroneID scanner/A10, Drone ID y BLE spam._
- **Apps:**
  - analyzer (`wifi_sniffer_begin` → `wifi_driver_init_null`)
  - captive, SSID spam
  - deauth (`wifi_driver_access_point_begin` always calls `wifi_driver_deinit()`)
  - deauth detector and DroneID scanner (`esp_wifi_stop()` on exit)
  - Modbus TCP and cat_dos (through `wifi_ap_manager`)
  - Drone ID (calls `esp_wifi_init()` directly)
  - rf_spectrum, wardriving (`wifi_driver_deinit`), BLE spam
- **Effects:**
  - `radio_lease_ble_acquire()` sees zero Wi-Fi claims and tears the driver down under these apps.
  - Their own teardown stops the driver under apps that do hold a lease.
  - No exit puts Wi-Fi back in its boot state.
- **Boot state:** `esp_wifi_init` done, STA mode, not started, promiscuous off, channel 1, default AP MAC, no event handlers.
- **What apps leave behind on exit:**

  | App | Wi-Fi state after exit |
  |---|---|
  | Analyzer | started in NULL mode on the sniffer channel |
  | Deauth | soft-AP still up (see A9) |
  | Modbus TCP, cat_dos | still associated to the target AP |
  | Detector, DroneID scanner | STA mode, stopped |
  | Captive, SSID spam | driver deinitialized |

- **Only partial restorers:** BLE spam, Zigbee, Thread, warbee and warthread call `wifi_service_init(STA)` on exit.
- **Hardware test (section 6):** the analyzer conversion in 79fd90b6 leaked one Wi-Fi claim per capture, which locked out BLE until reboot. Fixed in Phase 7.

- [x] **S4. The default netifs are created in many places, which can abort the device.** Confirmed.
- **STA netif creators:**
  - `wifi_controller` (twice, under two independent flags: [:80](../firmware/components/wifi_controller/wifi_controller.c#L80) and [:132](../firmware/components/wifi_controller/wifi_controller.c#L132))
  - [OTA wifi_ap.c:181](../firmware/components/OTA/modules/wifi_ap/wifi_ap.c#L181)
  - [wifi_ap_manager.c:77-79](../firmware/components/wifi_ap_manager/wifi_ap_manager.c#L77)
- **AP netif creators:** [id_open_esp32.cpp:198](../firmware/components/drone_id/id_open_esp32/id_open_esp32.cpp#L198), OTA, wifi_ap_manager.
- **No lookup guard:** all of the above except SSID spam and modbus_dos create without first checking whether the netif exists.
- **IDF behaviour:** IDF 5.5.1 rejects a duplicate interface key, and `esp_netif_create_default_wifi_*` then asserts, so the device aborts.
- **Examples:**
  - rf_spectrum, then deauth (`init_apsta` creates the STA netif again)
  - SSID spam, then a Modbus TCP connect or Drone ID
- **Fix:** one `ensure_default_netifs()` helper that checks `esp_netif_get_handle_from_ifkey()` first, as SSID spam already does.

- [x] **S5. The task registry fills up and then every task creation fails.** Confirmed.
- **Cause:** tasks created with `task_manager_create` that end with `vTaskDelete(NULL)` are never removed from the registry. This covers the analyzer's init, begin and out-of-memory tasks, the Wi-Fi sniffer task, the NMEA parser task, the UART bridge task, and cat_dos's send and HTTP tasks.
- **Effect:** each one permanently leaks a registry slot and its `strndup`'d name. The cap is 50 ([task_manager.c:66](../firmware/main/core/task_manager/task_manager.c#L66)). After that, `task_manager_create` returns `ESP_ERR_NO_MEM`, and the sniffer, GPS, wardriving, UART bridge and cat_dos stop working silently.
- **How fast:** about 3 slots per analyzer capture, and 1 per GPS session. The trackers app restarts GPS on every DOWN press.
- **Fix:** let `task_manager_delete(NULL)` mean "remove the calling task, then delete it", and use it in those tasks.

- [x] **S6. Tasks are force-deleted while they may hold a mutex or sit in a blocking driver call.** Plausible (timing-dependent). _Fixed for every site below (committed `2a8cc57d`..`347ced73`, plus cat_dos as part of A16, uncommitted); **hardware-verified 2026-11-27** for deauth, DroneID scanner, ieee_sniffer, wardriving and the deauth-detector hopper (section 10) and for cat_dos/A16 (section 12); captive/odrone exit-path only. Each now stops on a flag and is waited for via the new `task_manager_wait_exit()`; a force-delete remains only as a last resort after a timeout. The wardriving animation also dropped `vTaskSuspend` (which could freeze it holding `oled_mutex`) for a show flag. See the Phase 11 entry in section 5._
- **Background:** the repo already documents the hazard: deleting a task while it holds `oled_mutex` freezes the display forever (see `animations_task.c` and warthread).
- **Tasks still force-deleted while they may be drawing or writing:**
  - deauth's scan task
  - DroneID's process task (it draws the list)
  - `ieee_sniffer`'s debug task, which runs app callbacks ([ieee_sniffer.c:199](../firmware/components/ieee_sniffer/ieee_sniffer.c#L199))
  - the wardriving animation task
  - warthread's record task
  - the odrone spoofing task
  - captive's scan task
- **Tasks force-deleted mid-call:**
  - **Inside blocking Wi-Fi calls:** wardriving, deauth and captive scan tasks (in `esp_wifi_scan_start(block=true)`), the deauth detector's channel hopper (`set_channel`). The driver's per-task semaphore is freed when its task is deleted (IDF `esp_adapter.c:149-165`), but the driver may still signal it.
  - **Inside `connect()`:** cat_dos's HTTP task, which also leaks a socket (A16).
- **Fix pattern:** set a stop flag and wait for the task to exit, as trackers and animations_task already do.

---

## 2. Defects in specific apps

- [x] **A1. SSID spam frees a string literal, which aborts the device.** Confirmed.
- **Where:** `ssids_main_list[0]` starts as the literal `"List SSID"`. `list_ssid_cb` ([ssid_spam.c:154](../firmware/main/apps/wifi/ssid_spam/ssid_spam.c#L154)) and `on_exit` ([:434](../firmware/main/apps/wifi/ssid_spam/ssid_spam.c#L434)) call `free()` on it.
- **Effect:** the heap assert fires ([heap_caps_base.c:80](https://github.com/espressif/esp-idf/blob/v5.5.1/components/heap/heap_caps_base.c#L80)), and assertions are enabled in the sdkconfig. Picking an SSID list for the first time reboots the board.
- **Fix:** write the label into a static `char[32]`; no malloc or free.

- [x] **A2. Captive's `on_exit` deletes a task that no longer exists.** Confirmed.
- **Where:** `scanning_task` ([captive_module.c:862](../firmware/main/apps/wifi/captive/captive_module.c#L862)) deletes itself without clearing `scanning_task_handle`. Then `captive_on_exit` calls `vTaskDelete()` on that handle ([:1007](../firmware/main/apps/wifi/captive/captive_module.c#L1007)).
- **Effect:** after any clone-AP scan, exit deletes a freed task control block, or a different task that reused the handle.
- **Also:** `on_exit` frees `wifi_list` before stopping that task.
- **Fix:** clear the handle before the task's self-delete; stop the task first in `on_exit`.

- [x] **A3. Captive destroys the AP netif and leaves a dangling pointer in the Wi-Fi driver.** Confirmed.
- **Where:** `esp_netif_destroy("WIFI_AP_DEF")` ([:524](../firmware/main/apps/wifi/captive/captive_module.c#L524), [:990](../firmware/main/apps/wifi/captive/captive_module.c#L990)) runs without `esp_wifi_clear_default_wifi_driver_and_handlers()`.
- **Effect:** IDF's internal `s_wifi_netifs[WIFI_IF_AP]` keeps pointing at freed memory. The next AP start (deauth, Drone ID) dereferences it.
- **Also:** captive destroys a netif it never created, and never creates one, so DHCP and DNS only work if SSID spam or OTA ran earlier in the same boot.
- **Fix:** create the netif if missing; on exit use `esp_netif_destroy_default_wifi()`, or simply keep it for the life of the boot.

- [x] **A4. OpenThread is shut down while its main loop is still running.** Confirmed.
- **Where:** `openthread_deinit` ([open_thread.c:402](../firmware/components/open_thread/open_thread.c#L402)) calls `esp_openthread_deinit()` while `ot_task_worker` is inside `esp_openthread_launch_mainloop`. `esp_openthread_mainloop_exit()` (which exists in 5.5.1) is never called.
- **Effects:**
  - The loop keeps running on a finalized instance.
  - The cleanup code after the loop never runs: netif glue deinit, netif destroy, eventfd unregister.
  - `s_ot_task_handle` is set to NULL while the task is still alive.
  - **Plausible:** the next Thread entry starts a second worker, whose `esp_netif_new("OT_DEF")` hits a duplicate key and trips the assert at [:50](../firmware/components/open_thread/open_thread.c#L50).
- **Fix:**
  1. Call `esp_openthread_mainloop_exit()`.
  2. Wait for the worker task to finish.
  3. Do the teardown, including `esp_openthread_deinit()`, inside the worker.

- [x] **A5. `surv_radio_stop` releases leases before its task stops, including leases it doesn't hold.** Confirmed.
- **Where:** `surv_radio_start` (TRACKERS profile, [surv_radio.c:526](../firmware/components/surveillance_detect/surv_radio.c#L526)) and `surv_radio_stop` ([:549](../firmware/components/surveillance_detect/surv_radio.c#L549)) call `radio_lease_wifi_release()` unconditionally.
- **Effects:**
  - In SURVEIL, the Wi-Fi claim belongs to `radio_task` (acquired at [:491](../firmware/components/surveillance_detect/surv_radio.c#L491), released at [:497](../firmware/components/surveillance_detect/surv_radio.c#L497)). Stop steals it mid-window, then the task's own release underflows or steals another holder's claim.
  - In FLOCK, the result of the acquire is ignored but stop still releases.
- **Fix:** track Wi-Fi ownership with a flag, as `s_ble_claimed` already does. On stop: clear `s_running`, wait until `s_radio_task == NULL`, then release only the claims actually held.

- [x] **A6. Drone ID never releases the BLE lease.** Confirmed.
- **Where:** [id_open_esp32.cpp:275](../firmware/components/drone_id/id_open_esp32/id_open_esp32.cpp#L275) acquires it; [odrone_id.cpp:113](../firmware/components/drone_id/odrone_id.cpp#L113) never releases it, and the static `ble_initialized` flag is never reset.
- **Effect:** after one session the BLE reference count stays at 1 or more. `wifi_service_init`, `radio_lease_wifi_acquire` and `prom_acquire` are all refused until reboot, which breaks every lease-based Wi-Fi app.
- **Plausible:** `init_ble` also tears down the Wi-Fi that `init_w` just brought up, while id_open's `wifi_initialized` stays true. `set_wifi_ap()`'s `ESP_ERROR_CHECK(esp_wifi_set_mode)` would then abort. Needs a test on the device.
  - **Hardware test:** the teardown is confirmed. With the BLE drone enabled, Wi-Fi stays down for the whole run, so the Wi-Fi spoofer sends nothing. No abort was seen.
- **Plausible:** `odrone_id_set_num_spoofers` (`delete[]` at [:45](../firmware/components/drone_id/odrone_id.cpp#L45)) frees the spoofer array while the spoofing task may still be iterating it.

- [x] **A7. wifi_ap_manager leaks handlers and stays connected after exit.** Confirmed.
- **Handlers:** registered at [wifi_ap_manager.c:86](../firmware/components/wifi_ap_manager/wifi_ap_manager.c#L86) and never unregistered. `wifi_ap_manager_deinit()` has no callers.
- **Stale flag:** `initialized` stays true after another module deinits the driver. The next join then hits `ESP_ERROR_CHECK(wifi_service_set_mode)` (L123) and aborts.
  - Example: Modbus TCP connect, then BLE spam, then Modbus TCP connect again.
- **Leftover handler:** any later STA disconnect from any app triggers reconnect attempts and wipes the saved `ssid`/`passwd` preferences.
- **No disconnect:** Modbus TCP `on_exit` ([:453](../firmware/main/apps/wifi/modbus_tcp/scenes/modbus_tcp_scenes.c#L453)) and cat_dos `on_exit` ([:524](../firmware/main/modules/cat_dos/catdos_module.c#L524)) never disconnect, so the device stays associated to the target AP.
- **Fix:** a real deinit (unregister handlers, disconnect, reset the flag), called from both exits.

- [x] **A8. The analyzer and sniffer leak on several paths.** Confirmed. _Done: mutex en 269fe1c2; flash-limit, fallo de arranque y tablas de comandos en Phase 9. El caso plausible de `summary_mutex`/`state_mutex` ya no aplica: `wifi_analyzer_on_exit` conserva los mutex durante todo el arranque (la begin/init/limit-handler aún pueden tomarlos), en vez de borrarlos._
- **Flash limit:** the flash-limit `break` ([cmd_sniffer.c:338](../firmware/components/wifi_sniffer/cmd_sniffer.c#L338)) skips `free(payload)` ([:350](../firmware/components/wifi_sniffer/cmd_sniffer.c#L350)), leaking one packet buffer (up to 2346 B) each time.
- **Start failure:**
  - **Trigger:** the error labels in `sniffer_start` ([:430](../firmware/components/wifi_sniffer/cmd_sniffer.c#L430)) never undo `sniff_packet_start`, so `is_writing` stays true. `wifi_sniffer_start` ignores the result ([wifi_sniffer.c:68](../firmware/components/wifi_sniffer/wifi_sniffer.c#L68)), so the app thinks the capture is running.
  - **Close step:** `--close` then refuses, the FILE and pcap session leak, and the SD card is unmounted with the file still open ([:84](../firmware/components/wifi_sniffer/wifi_sniffer.c#L84)).
  - **When:** whenever BLE holds the radio, or the registry is full (S5).
  - **Fix:** call `sniff_packet_stop()` on the error path and propagate the error.
- **Command tables:** `wifi_sniffer_begin` ([:42-43](../firmware/components/wifi_sniffer/wifi_sniffer.c#L42)) re-allocates its argtable entries on every analyzer begin, and every capture's summary exit triggers a re-begin. About 10 small allocations leak per capture. Fix: register once.
- **Plausible — resolved:** `wifi_analyzer_on_exit` ([:534](../firmware/main/apps/wifi_analyzer/wifi_analyzer.c#L534)) no longer deletes `summary_mutex` and `state_mutex`; it keeps them for the life of the boot, since the begin, init or limit-handler tasks may still take them (a deleted mutex there would be `xSemaphoreTake(NULL)`).

- [x] **A9. Deauth leaves the soft-AP broadcasting after exit.** Confirmed.
- **Where:** `deauth_module_exit` ([:293](../firmware/main/apps/wifi/deauth/deauth_module.c#L293)) and `wifi_attacks_module_stop` ([:166](../firmware/components/wifi_attacks/wifi_attacks.c#L166)) stop the tasks and restore the MAC, but never stop the AP.
- **Effect:** the rogue AP (cloned SSID) or the manager AP keeps beaconing on the target channel.

- [x] **A10. DroneID scanner tears down in the wrong order and hides a startup failure.** Confirmed.
- **Order:** `droneid_scanner_stop` ([:267](../firmware/main/apps/wifi/droneid_scanner/droneid_scanner.c#L267)) deletes the packet queue before releasing the promiscuous lease. The higher-priority Wi-Fi callback can send to the freed queue in that window. Fix: release the lease, then the tasks, then the queue.
- **Startup:** a failed promiscuous claim returns `err`, which is still `ESP_OK` ([:231](../firmware/main/apps/wifi/droneid_scanner/droneid_scanner.c#L231)), so the tasks start anyway.

- [~] **A11. rf_spectrum closes the pcap file before stopping writes to it.** Confirmed (narrow window).
- **Where:** `rf_spectrum_set_pcap_enabled(false)` ([:693](../firmware/main/apps/rf_spectrum/rf_spectrum.c#L693)) closes the FILE before clearing `pcap_logging`, while the Wi-Fi callback is still writing to it.
- **Fix:** clear the flag first.
- **Also:** the driver is brought up outside the lease and never released.

- [x] **A12. warthread frees its buffers before stopping their users.** Confirmed.
- **Where:** `warthread_module_exit` ([:369](../firmware/main/modules/war_thread/warthread_module.c#L369)) frees the CSV buffer, filename and session string and unmounts the SD card ([:388](../firmware/main/modules/war_thread/warthread_module.c#L388)) before `ieee_sniffer_stop` ([:403](../firmware/main/modules/war_thread/warthread_module.c#L403)) and before stopping the record task.
- **Effect:** the record task can write through a freed filename, double-free the buffer, or write to an unmounted card.
- **Fix order:**
  1. Stop the sniffer.
  2. Stop the record task and wait for it.
  3. Delete the queue.
  4. Flush, free, unmount.

- [x] **A13. Wardriving shuts Wi-Fi down while its scan task is mid-scan.** Confirmed.
- **Where:** `wardriving_module_stop_scan` deinits Wi-Fi ([:448](../firmware/main/modules/gps/wardriving/wardriving_module.c#L448)) while the scan task, an endless `while(true)` loop, may be inside a blocking scan. It then force-deletes the task (S6).
- **Fix:** make the loop flag-driven, wait for it to exit, then release the radio.

- [x] **A14. A GPS start failure blocks GPS until reboot.** Confirmed.
- **Where:** `gps_module_start_scan` sets `is_uart_installed = true` ([:125](../firmware/main/modules/gps/gps_module.c#L125)) before `nmea_parser_init`.
- **Effect:** if init returns NULL (for example because of S5), the stop path returns early and the flag stays set.
- **Fix:** set the flag only on success.

- [x] **A15. The UART bridge can miss an exit and doesn't restore UART0.**
- **Lost exit (confirmed race):** the task resets `uart_bridge_exit = false` ([:58](../firmware/main/apps/gpio/uart_bridge/uart_bridge_app.c#L58)). If exit is pressed before this low-priority task first runs, the bridge keeps running with the console paused.
- **UART0 not restored (confirmed):** `uart_bridge_end` deletes the UART0 driver that `main.c` installed at boot.
  - **Effect:** logs-to-UART and the console UART commands die until reboot.
  - **Fix:** reinstall the boot configuration on exit.

- [x] **A16. cat_dos joins threads that can never finish, and leaks sockets.** Confirmed, **hardware-verified 2026-11-27 (section 12)**.
- **Where:** `http_get_task` runs as 8 pthreads but ended with `task_manager_delete(NULL)`/`vTaskDelete(NULL)`, so the pthread completion path never ran and `pthread_join` ([:251](../firmware/main/modules/cat_dos/catdos_module.c#L251)) blocked forever. Each attack leaked the sender task and 8 pthread descriptors.
- **Also:** `on_exit` force-deleted the attack task. If that happened during `connect()`, the socket and `addrinfo` leaked, and lwIP has only about 10 sockets.
- **Fix (uncommitted, build OK, hardware-verified — section 12, with a deterministic target; the real-AP checks from 8.3 are still blocked on a Wi-Fi join that timed out):** `http_get_task` now always returns normally and never self-deletes, so it only ever runs as a pthread body and ESP-IDF's pthread wrapper can signal `pthread_join()`. The redundant extra `task_manager_create(http_get_task, ...)` FreeRTOS task (`task_atack`, never joined by anything, the task that was actually being force-deleted) is removed — the 8 pthreads already do that work. `catdos_on_exit` now tracks `catdos_module_send_attack_task` itself (`attack_task_handle`) and calls `task_manager_wait_exit(&attack_task_handle, 5000)` so it can finish its `pthread_join()` chain and close its sockets on its own, same pattern as the S6 sites; force-delete remains only as a last resort after the timeout (ponytail: a thread stuck past 5 s in a long lwIP `connect()` retry can still be orphaned by that fallback — add a connect timeout via `setsockopt`/non-blocking connect if that proves to matter in practice).

- [x] **A17. Console `launch` and `sdr_capture` start a new app without ending the current one.** Confirmed.
- **Where:** `menus_module_set_menu` ([:311](../firmware/main/modules/menus_module/menus_module.c#L311)) switches menu without exiting the running app.
- **Fix:** in `launch_app_task` ([cmd_control.c:32](../firmware/main/modules/cmd_control/cmd_control.c#L32)), run the same exit logic as `exit_app_cmd` first.

- [x] **A18. Smaller issues.** _All four items done, committed; #1, #3 and #4 (normal path) hardware-verified 2026-11-27 (section 11). #2 cannot be exercised on hardware._
- [x] **BLE spam double stop (confirmed):** `ble_spam_stop` runs twice on exit, and `bt_spam_app_deinit` unblocks scanning unconditionally, so the second call drops another app's scan block. _Fixed (uncommitted, build OK, not tested on hardware): `bt_spam.c` keeps a `bt_spam_scan_blocked` flag set after `radio_lease_ble_block_scan()` and unblocks only while it is set (both in `bt_spam_app_deinit` and the `bt_spam_app_main` error path)._
- [x] **BLE spam latent mutex bug (confirmed, latent):** `ble_spam_on_exit` deletes `text_mutex` without setting it to NULL. It isn't reachable today, but becomes live once the app wrappers are wired up. _Fixed (uncommitted, build OK, not tested on hardware): `ble_spam_on_exit` sets `text_mutex = NULL` before deleting the handle._
- [x] **`bt_gattc_task_stop` releases the BLE stack right after the asynchronous `gattc_app_unregister`; HID explicitly waits for that step.** _Fixed (uncommitted, build OK, not tested on hardware): `bt_gattc.c` waits up to 500 ms for `ESP_GATTC_UNREG_EVT` (flag set in `ble_client_esp_gattc_cb`) before unregistering the dispatcher and releasing the lease; logs a warning if it never arrives._
- [x] **Modbus TCP and `ieee_sniffer` worker tasks set their own running flag when they start, so a stop issued before they first run is lost.** _`ieee_sniffer`: fixed (uncommitted, build OK, zigbee host tests pass, not tested on hardware). New `ieee_sniffer_arm()` sets `running` before `xTaskCreate` at the four call sites (zigbee sniffer, zigbee mesh mapper, warthread, warbee); `ieee_sniffer_begin` no longer sets it and exits at once if a stop already cleared it. Modbus TCP: nothing to fix — `modbus_dos.c`'s `is_running` has no readers (the loop in `reading_task` is commented out) and the keep-alive task is commented out too._

---

## 3. Cleanup routines that are missing entirely

- [x] **OTA:** `on_exit` ([ota_module.c:49](../firmware/main/modules/ota/ota_module.c#L49)) is empty and says there is nothing to free. In fact `OTA_init` starts:
  - a Wi-Fi task
  - a soft-AP
  - 2 event handler instances
  - an HTTP server and its monitor task
  - the STA and AP netifs
- [x] **wifi_ap_manager:** no working deinit (see A7).
- [x] **wifi_sniffer:** nothing to undo. `wifi_sniffer_begin` registers the argtables once per boot (guarded by a `static bool`), and the filter hashtable is a static array, not heap. A deinit that freed them would be wrong, since `do_sniffer_cmd` still uses them on the next capture and they are never re-registered. The driver bring-up moved into the analyzer's `radio_lease` claim (S3).
- [x] **odrone_id:** no BLE release, no reset of its init flags.
- [x] **Deauth:** nothing stops the AP.
- [x] **UART bridge:** nothing restores the boot UART.
- [x] **task_manager:** no way for a task to remove itself.
- [~] **App lifecycle:**
  - [x] A shutdown handler now unmounts the SD card on any deliberate `esp_restart()` (commit `c0f2b880`), covering the data-loss path in section 4. **Hardware-verified 2026-11-27 (section 10):** the handler runs on `reset` and `sd_card_unmount()` returns `ESP_OK` with a normal reboot.
  - [ ] `error_handler` is initialised in `main.c` but otherwise dead: none of the `ERROR_REPORT`/`ERROR_WIFI`/`ERROR_CRITICAL` macros are used, no callback or restart callback is set. Wiring it through the tree is a large change with no caller demanding it (YAGNI); left as-is pending a decision to either use or remove the subsystem.
  - [ ] `app_manager_watchdog_*` are stubs with no callers; `app_manager_feed_watchdog()` does nothing. A real task-watchdog integration is a config/policy decision (`CONFIG_ESP_TASK_WDT_PANIC` is off), not wired up here.

## 4. Watchdog, crash, restart and sleep paths

- **Watchdog and crashes:** the task watchdog only logs (`CONFIG_ESP_TASK_WDT_PANIC` is off), so it never resets the board. A panic reboots with no cleanup at all.
- **What a reboot or deliberate restart loses:**
  - data buffered in RAM: surveillance CSV/GPX, warbee and warthread CSV, and up to 50 unflushed pcap packets
  - **Plausible:** the FAT file system can be left mid-write.
  - Deliberate restarts (web file browser, Zigbee switch/light, SDR exit) skip unmounting the SD card.
- **Persisted state:** `MENUNUMBER` and `wifi_connected` are reset at boot, so nothing stale survives a reboot.
- **Deep sleep:** entered from the idle timer ([sleep_mode.c:28](../firmware/main/modules/settings/sleep_mode/sleep_mode.c#L28)) whenever no app is active. Because of S1 and S3, the radio can still be up when that happens. Low risk.

## 5. Clean paths, coverage and suggested order

**Clean paths:** bt_spam, BLE HID, trackers, animations_task, the SDR's lease handling (it pairs every claim with a release and reboots on exit), and warbee's teardown order.

**Not covered:** Zigbee switch/light internals (they exit by reboot), the file manager's coroutine, the gattcmd console services, and the settings screens.

**Suggested order:**
1. Phase 1: S1 together with the exits it activates (commit 269fe1c2).
   - [x] S1
   - [x] A1
   - [x] A2
   - [x] A8, mutex issue only
2. Phase 2: Wi-Fi state and netifs (commit 21454673).
   - [x] S2
   - [x] S4
   - [x] A3 (netif destroy folded in, since it breaks S4's keep-for-the-boot rule)
3. Phase 3: lease and handler leaks.
   - [x] A6 (commit 65754e7c)
   - [x] A7 (commit d1f070e5)
4. Phase 4: task registry (commit c18f82ab).
   - [x] S5
5. Phase 5 (uncommitted):
   - [x] A4 (the repo vendors `components/openthread`, which lacked `esp_openthread_mainloop_exit()`; backported there)
   - [x] A5
6. Phase 6 (uncommitted): route the clean Wi-Fi apps through `radio_lease`.
   - [~] S3 (clean apps only: analyzer, captive, SSID spam, deauth detector, shared `wifi_scanner`)
7. Phase 7 (uncommitted): fixes found by the hardware test (section 6).
   - [x] H1: analyzer takes its Wi-Fi claim once per session ([wifi_analyzer.c:230](../firmware/main/apps/wifi_analyzer/wifi_analyzer.c#L230))
   - [x] H2: HID back calls `app_manager_exit()` only ([hid_module.c](../firmware/main/apps/ble/hid_device/hid_module.c))
   - [x] H3: Flock back calls `app_manager_exit()` instead of `surveillance_module_stop()` ([surveillance_module.c](../firmware/main/apps/surveillance/surveillance_module.c))
8. Phase 8 (uncommitted, build OK, not tested on hardware):
   - [x] H4, H5
   - [x] A10 (release lease, then tasks, then queue; failed promiscuous claim now returns an error)
   - [x] A14 (flag set only after `nmea_parser_init` succeeds)
   - [~] A11 (pcap flag cleared before the file closes; the lease for the driver bring-up is still open)

9. Phase 9 (uncommitted, build OK, not tested on hardware):
   - [x] A8: `free(payload)` on the flash-limit break; `sniff_packet_stop()` on the `sniffer_start` error path; `do_sniffer_cmd` and `wifi_sniffer_start` propagate the failure (NO_MEM stays with `out_of_mem_cb`); `wifi_sniffer_begin` registers the argtables once
   - [x] A9: `deauth_module_exit` calls `wifi_driver_deinit_if_started()` after stopping the attacks
   - [x] A17: `launch` and `sdr_capture` run the exit logic first (`exit_current_app()`, shared with `exit`)
   - [x] A12: warthread exit order is now sniffer, record task (waits up to 1 s), queue, GPS, then SD unmount and frees
   - [x] A13: the wardriving scan loop is flag-driven and exits through `task_manager_delete(NULL)`; stop waits for it and no longer calls `wifi_driver_deinit()` (each scan already holds a `radio_lease` claim via `wifi_scanner`)
   - [x] A15: the bridge no longer clears `uart_bridge_exit` in the task (lost-exit race) and reinstalls the boot UART0 configuration on exit
   - [x] Section 3, OTA: new `wifi_app_stop()`/`OTA_deinit()` stop the HTTP server, task, queue, event handlers and soft-AP. The menu entry had `on_exit_cb = NULL`, so it now points at `ota_module_exit`

**Phase 10 (uncommitted, hardware-tested):** fixes for the defects found in section 8.
- [x] H7: deauth holds an APSTA `radio_lease` claim for the whole session; `wifi_driver_access_point_begin()` no longer deinitialises on entry; exit releases the claim (section 8.4)
- [x] H8: `sniffer_start` claims promiscuous mode before creating the sniffer task, so a failed claim never force-deletes a drawing task (section 8.4)
- [x] H9: `rf_spectrum_set_pcap_enabled()` tests `rf_spectrum_pcap_start() == ESP_OK` (section 8.4)
- [x] A9 (folded into H7: deauth's exit now stops the AP because it holds the claim)
- [x] A11 (folded into H9: pcap logging turns on, and toggling off while frames arrive no longer crashes; the driver bring-up still has no lease)

**Not yet assigned to a phase:**
- [~] S3 (rest: wardriving/A13, rf_spectrum/A11 lease, DroneID scanner/A10, Drone ID, BLE spam; deauth/A9 done in Phase 10; Modbus TCP + cat_dos **reverted**: committed as `c51229bf`, reverted in `825f8f18`, so `wifi_ap_manager` brings the driver up on its own again. Revert reason not recorded; investigated 2026-11-27 — no bug found in the diff, most likely backed out because it could not be hardware-verified without a real AP (full finding in 8.3))
- [x] S6 (Phase 11, committed `2a8cc57d`..`347ced73`, host tests pass, **hardware-verified 2026-11-27, section 10** (plus cat_dos/A16, same pattern, section 12)). New `task_manager_wait_exit(&handle, timeout_ms)` (`2a8cc57d`): waits for a task that clears its own handle before deleting itself; returns false at once when called from that task. Each site below now stops on a flag and is waited for; a force-delete with `ESP_LOGE` stays only as a last resort after the timeout:
  - deauth scan task (5 s, `60a53745`): stops between scans and skips the menu redraw when aborted (`deauth_stop_scan_task()`, used by LEFT and by `deauth_module_exit`)
  - captive scan task (5 s, `b244ef5d`): same; it now always releases its `radio_lease` claim (a force-delete used to leak it)
  - deauth detector channel hopper (`449253d8`, 2 s, delay in 100 ms slices)
  - DroneID scanner (`d5f8c5bc`, 2 s each): both tasks loop on `scanner_running`, the process task uses a 100 ms queue timeout, and `esp_wifi_stop()` now runs after they exit. This also fixes the hopper spinning with no delay when `esp_wifi_set_channel` failed on the stopped driver
  - `ieee_sniffer` debug task (`662581fc`, 2 s, inline wait: the component cannot see `task_manager`): loops on `running` with a 100 ms queue timeout, and the queue is deleted only after it exits
  - wardriving animation task (`953aad6a`, 1.5 s): **new finding** — `vTaskSuspend` from the GPS callback could suspend it inside an OLED call holding `oled_mutex`, and the GPS task's next draw then blocked forever. Suspend/resume replaced by a show flag, and the task loop moved to `wardriving_module.c` (`wardriving_screens_wifi_animation_frame()` draws one frame)
  - odrone spoofing task (`347ced73`, 2 s, inline wait): loops on `s_spoofing_running`, and the spoofers are freed only after it exits
  - Not changed at the time: warthread record task (already waits, A12); cat_dos, fixed separately as A16
  - **Hardware checks pending:** enter/exit each app above, including LEFT during a scan (deauth, captive), a drone list update (DroneID), and no GPS fix ↔ fix (wardriving). Pass: menu drawn, no "did not stop" log, `wifi_refs`/`prom_refs`/`tm` back to baseline
- [x] A8, rest (`wifi_analyzer_on_exit` mutex deletion): already fixed in the tree — `wifi_analyzer_on_exit` keeps the mutexes for the life of the boot, since the begin, init and limit-handler tasks may still take them
- [x] A16 (uncommitted, build OK, **hardware-verified 2026-11-27, section 12**; the real-AP checks in 8.3 are still blocked on a Wi-Fi join that timed out)
- [x] A18 (committed, hardware-verified 2026-11-27, section 11; #2 stays latent)
- [x] Section 3: wifi_sniffer teardown (nothing to undo — see section 3); SD-unmount shutdown handler (`c0f2b880`, hardware-verified section 10). Remaining Section 3 lifecycle items (`error_handler` wiring, watchdog) left as deliberate non-work — see section 3

---

## 6. Hardware verification (2026-11-26)

- **Device:** Minino (ESP32-C6), USB-Serial-JTAG console on `/dev/ttyACM0`.
- **Firmware:** HEAD `a0fafa87`, then the same build plus the Phase 7 fixes. ESP-IDF v5.5.1.
- **Method:**
  - A script drove the console: `launch <app>`, `press <button> [event]` and `display` (which reports whether anything is drawn, the current menu and `in_app`).
  - Opening the port resets the board, so every run starts from a clean boot.
  - A temporary console command, `radio_state`, printed the `radio_lease` Wi-Fi/BLE/promiscuous counts, `wifi_service` up, the driver's mode, started and promiscuous state, the rf_spectrum RSSI, the `task_manager` count and the FreeRTOS task count. It was removed after testing.
  - A run fails on any panic, abort, assert or reboot.
- **Not tested on hardware:**
  - A7 (needs a real AP to join).
  - A9 (needs a running deauth attack).
  - Every item still open in the list above.

### 6.1 Results for the fixed items

| Item | Result | What the board showed |
|---|---|---|
| S1 | ✅ after Phase 7 | Analyzer, captive, deauth detector, Drone ID, DroneID scanner, SSID spam, cat_dos, I2C scanner and file manager return to a drawn menu on LEFT. OTA does too, on LEFT single-click, its only exit event. HID and Flock failed at HEAD (H2, H3) and pass after Phase 7. |
| A1 | ✅ | Picked a stored SSID list and exited, twice. No abort, heap stable (about 186 KB). |
| A2 | ✅ | Replicate-mode clone scan, then back from the AP list and from the app. The scan task ends and releases its claim, and the task count returns to baseline. |
| A3 | ✅ | Captive, then deauth: the AP starts and the scan returns 12 APs, with no fault from a dangling netif. |
| S2 | ✅ | Analyzer, then Zigbee sniffer, then rf_spectrum: the driver is started, in promiscuous mode, with RSSI -69 dBm on ch 1. |
| S4 | ✅ | SSID spam then Drone ID Run, and rf_spectrum then deauth: no duplicate-netif assert. |
| A4 | ✅ | Thread sniffer entered and exited 3 times: no `OT_DEF` assert, and the task count returns to 9 after each exit. Heap drifts about 190 B per session. |
| A5 | ✅ | 30 Flock↔Trackers switches: Wi-Fi and promiscuous counts stay at 1 in Flock, with no "release without a matching acquire" warning. Both are 0 after exit. |
| A6 | ✅ | Drone ID Run takes the BLE claim (`ble_refs=1`) and stop returns it to 0. The analyzer then gets its Wi-Fi claim. |
| A7 | not tested | Needs an AP to join. |
| S5 | ✅ | `task_manager` count is 0 at rest after 15 analyzer captures and after 30 GPS restarts (the old code would have leaked about 75 slots). |
| S3 (converted apps) | ✅ after Phase 7 | Captive, SSID spam, deauth detector and `wifi_scanner` release their claims. The analyzer leaked one claim per capture at HEAD (H1). |

### 6.2 Defects found

| ID | Severity | Defect | Status |
|---|---|---|---|
| H1 | High, regression from 79fd90b6 | Every summary exit re-runs the analyzer's setup task, and it acquired again even when `wifi_leased` was already set. After 15 captures, `wifi_refs` was 16, and still 16 after exit. After a single capture, `radio_lease_ble_acquire()` refused (`BLE requested while WiFi has 1 holder(s)`), so trackers, HID, BLE spam and the Drone ID BLE drone stayed locked out until reboot. | Fixed (Phase 7). `wifi_refs` stays at 1 over 6 captures and is 0 after exit; trackers then gets BLE. |
| H2 | Medium, S1 incomplete | HID back cleared `in_app` (`menus_module_set_default_input()`) before `app_manager_exit()`, so the fallback skipped `menus_module_exit_app()`. The screen stayed blank on menu 37, and only a second LEFT recovered. | Fixed (Phase 7). Back from the menu and from pairing both draw the parent menu, and BLE is released. |
| H3 | Medium, not in the audit | Flock back called `surveillance_module_stop()`, which clears `in_app` and the screen, and nothing returned to the menu. The "app_manager_exit() will be called by on_exit" comment was wrong. Trackers was not affected. | Fixed (Phase 7). Teardown runs through the menu's `on_exit_cb` and the menu is drawn. |
| H4 | High, predates this branch | OTA aborts on entry. `CONFIG_WIFI_AP_PASSWORD` defaults to `""` in `minino_config/Kconfig.projbuild`, so the `"Cats1234"` fallback in `wifi_ap_config.h` never applies. WPA2 then rejects the empty password and `ESP_ERROR_CHECK(esp_wifi_set_config)` aborts ([wifi_ap.c:219](../firmware/components/OTA/modules/wifi_ap/wifi_ap.c#L219)). | Fixed (Phase 8): Kconfig default is now `Cats1234`, and `wifi_ap.c` falls back to an open AP if the password is empty. Not yet tested on hardware. |
| H5 | Medium, predates this branch | SSID spam with no stored lists: opening "List SSID" and pressing RIGHT calls `select_cb(0)` on an empty submenu. `strlen(NULL)` at [ssid_spam.c:151](../firmware/main/apps/wifi/ssid_spam/ssid_spam.c#L151) then causes a load access fault and a reboot. | Fixed (Phase 8): `list_ssid_cb` returns on a NULL entry. Not yet tested on hardware. |
| H6 | Build | `managed_components` re-resolved esp-modbus to 2.1.4, which statically asserts `CONFIG_FMB_MASTER_MAX_API_BLOCKING_TIME_MS >= CONFIG_FMB_MASTER_TIMEOUT_MS_RESPOND` (6000 < 10000), so the build fails. `dependencies.lock` and `sdkconfig` are gitignored, so fresh clones hit this too. | **Fixed (commit `7ae2f9ea`).** esp-modbus was a dead dependency — nothing in the tree includes or links it, and the Modbus TCP app speaks Modbus over raw sockets. Removing `espressif/esp-modbus` from `main/idf_component.yml` drops the component and the static assert with it; no `sdkconfig.defaults`/version pin needed. Local build passes and the regenerated `dependencies.lock` no longer lists it. **Pending:** a build from a truly fresh clone (empty `managed_components`/no lock) to confirm the resolver no longer pulls esp-modbus in. |

### 6.3 Behaviour matching items still open

- **OTA (section 3):** after exit the soft-AP stays up (APSTA, started) and 4 tasks remain. A later Wi-Fi app's release deinitializes the driver under OTA's HTTP server, without crashing.
- **rf_spectrum (A11):** leaves the driver started in STA mode with no claim held.
- **DroneID scanner (S3 rest):** releases its promiscuous claim, but leaves the driver initialized in NULL mode, stopped.
- **Analyzer heap:** drops about 800 B per capture (the A8 command tables).
- **Boot state:** S3 describes it as STA mode, but `wifi_service_init()` never calls `esp_wifi_set_mode()`. The driver reports AP mode at boot while the service records STA. Harmless, since every app sets the mode, but S3's description of the boot state is only true of the service.
- **After an analyzer exit:** the last-claim release deinitializes the driver (`svc_up=0`) rather than restoring the boot state. This is by design in `radio_lease_wifi_release()`, and later apps bring the driver up again.

### 6.4 Correction to section 5

"Clean paths" lists BLE HID. On the device its back button left a blank screen (H2), so it was not clean. Trackers is confirmed clean.

## 7. Pending hardware tests (Phases 8 and 9)

Everything below builds but has not run on the board. Use the section 6 method (console script, clean boot per run, a run fails on any panic, abort, assert or reboot). The temporary `radio_state` command was removed, so re-add it locally to read the `radio_lease` counts, `wifi_service` up, the driver's mode, the `task_manager` count and the FreeRTOS task count.

Pass criteria that apply to every test: no panic, abort or reboot; the parent menu is drawn after LEFT; the `task_manager` count and the lease counts return to their values at rest; free heap does not keep dropping across repeated runs.

### 7.1 Phase 9

| ID | Test | Expected |
|---|---|---|
| A8 flash limit | Run the analyzer until the flash packet limit is hit (lower the limit in sdkconfig if needed). Repeat 3 times. | Limit screen shows, the analyzer exits, and free heap does not drop by about one packet buffer (up to 2346 B) per run. |
| A8 start failure | Hold the BLE claim (open trackers or BLE spam, then enter the analyzer through the console `launch`), or fill the task registry. Start a capture. | The analyzer returns to its menu and does not report a running capture. The pcap file is closed (it opens in the file manager), and `--close` is not refused. Out-of-memory still shows the "no memory" screen once, not twice. |
| A8 command tables | 15 analyzer captures in a row. | Heap drop per capture is far below the old 800 B. |
| A9 deauth AP | Start a rogue-AP attack and a broadcast attack, then press LEFT out of the app. Scan for Wi-Fi from a phone. | The cloned SSID and the manager AP disappear. Driver is down (`svc_up=0`). Entering the analyzer or Flock afterwards gets its Wi-Fi claim. |
| A9 repeat | Deauth, exit, deauth again, 5 times. | No duplicate-netif assert (S4), scan still finds APs. |
| A17 console | `launch ssid_spam`, then `launch deauth` without `exit`. Repeat with the analyzer, captive and HID. Also `launch` followed by `sdr_capture`. | The first app's cleanup runs (leases back to 0, task count back to baseline) before the second one starts. No stacked apps. |
| A17 no app | `launch <app>` from the main menu, and `exit` with no app open. | `launch` still works; `exit` still prints "Not currently inside an application." |
| A12 warthread | Start warthread with GPS and SD, capture some frames, press LEFT. Repeat 5 times, also pressing LEFT right after entering. | No panic, the CSV on the SD card is readable and not truncated, the OLED is not frozen (no stuck `oled_mutex`), the SD card remounts on the next run. |
| A13 wardriving | Start wardriving, wait for at least one scan, press LEFT mid-scan. Repeat 5 times. | The exit takes at most about 3 s with no blocked UI, `wifi_refs` is 0 afterwards, the `task_manager` count returns to baseline, the final CSV is appended. Entering the analyzer or deauth afterwards works. |
| A15 UART bridge | Open the UART bridge, press LEFT right away (before it first runs) and again after sending data both ways. | The bridge never keeps running with the console paused. After exit the console answers commands, logs-to-UART still print (with the logs output setting on UART), and the `uart_bridge_get_config` console command reports the boot values (115200 8N1), not the bridge settings. |
| OTA exit | Enter OTA, join its soft-AP from a phone, press LEFT (single click). | The AP disappears, `svc_up=0`, the 4 leftover tasks are gone, the HTTP server no longer answers. |
| OTA re-entry | Enter and exit OTA 5 times, then flash an update once. | No duplicate-netif or event-handler assert, the AP comes back each time, a full update still completes and reboots. |
| OTA then another app | OTA, exit, then analyzer, Flock and BLE spam. | Each gets its lease (the soft-AP is no longer under them). |

### 7.2 Phase 8 (still untested)

| ID | Test | Expected |
|---|---|---|
| H4 | Enter OTA from a fresh clone build (default sdkconfig). | No abort; the AP is up with password `Cats1234`. Then with `CONFIG_WIFI_AP_PASSWORD=""`: the AP is open and does not abort. |
| H5 | SSID spam with no stored lists: open "List SSID" and press RIGHT. | Nothing happens, no reboot. |
| A10 | DroneID scanner: run, exit, repeat 10 times; also start it while BLE holds the radio. | No crash on exit (queue freed after the lease). The failed claim is reported and the tasks do not start. |
| A14 | Start GPS while the task registry is full or the UART is busy, then start it again. | The second start works (flag not left set). |
| A11 | rf_spectrum with pcap logging on, toggle it off while frames arrive. | No crash or corrupt pcap. Note: the driver bring-up still has no lease, so it stays started after exit (section 6.3). |

### 7.3 Regression sweep after the tests

Run once with the full set of fixed apps, in this order, with no reboot in between: analyzer, deauth, captive, SSID spam, OTA, warthread, wardriving, UART bridge, trackers, Flock, BLE spam. After each exit check `wifi_refs`, `ble_refs`, `prom_refs`, the `task_manager` count and free heap. All counts must be 0 or at baseline at the end, and heap must stay within about 1 KB of the starting value.

### 7.4 Needs equipment or an environment

- **A7:** Modbus TCP and cat_dos need a real AP (or `modbus_sim.py` behind one). After exit the device must be disconnected and a later STA disconnect must not trigger reconnects or wipe `ssid`/`passwd`.
- **A9 with a target:** needs a second device to confirm the deauth frames actually stop.
- **OTA update:** needs a phone or laptop to join the AP and upload a `.bin`.
- **A12/A13:** need a GPS fix (outdoors, or a GPS simulator on the UART).

## 8. Hardware verification of Phases 8 and 9 (2026-11-26)

- **Device:** the same Minino (ESP32-C6) on `/dev/ttyACM0`, ESP-IDF v5.5.1.
- **Firmware:** the working tree (Phases 8 and 9, uncommitted), with temporary test hooks in `cmd_control.c`: `radio_state` (now also reports SPIFFS usage), `press_after <ms> <button>` and `loglvl <tag> <n>`. They are not part of the fix and must be removed before committing.
- **Local sdkconfig:** `CONFIG_WIFI_AP_PASSWORD=""`, so H4 was tested on its open-AP fallback, not on the new `Cats1234` default.
- **Method:** as in section 6, with these changes:
  - Each run starts with a hard reset over RTS. Opening the port alone did not always reset the board.
  - The laptop's Wi-Fi adapter (`nmcli` rescans) was used to check whether the board's APs were still on the air.
  - A run fails on a panic, abort, assert, reboot, or a console that stops answering.
- **Not done, needs equipment or a decision:** see 8.3.

### 8.1 Results

| ID | Result | What the board showed |
|---|---|---|
| H4 | ✅ (fallback only) | OTA starts. `MININO_AP` is up and open (no security), with no abort. The `Cats1234` default was not tested. |
| H5 | ✅ | No stored SSID lists. "List SSID", then RIGHT: nothing happens, no reboot. |
| A10 | ✅ | 10 runs: no crash. Promiscuous count 1 → 0 and FreeRTOS task count back to 8 each time, heap flat (about 40 B per run). With BLE held (`gattcmd_scan`): `WiFi stack init failed` is reported and no tasks start. The lease guard refuses the Wi-Fi bring-up before the promiscuous claim is reached. |
| A14 | ✅ (normal path only) | GPS start, stop, start: both starts work. Forcing an NMEA init failure needs a full task registry or a busy UART. That would take a code hook, since A17 no longer lets two apps run at once. |
| A11 | ⚠️ blocked by H9 | `spectrum_pcap on` never enables logging (H9), so the close-while-writing race can't be exercised. 20 on/off and toggle cycles: no crash. |
| A8 flash limit | ✅ | Limit temporarily lowered to 40 packets (now restored to 900), destination Internal (now set back to SD). The limit screen shows, then the summary, then the analyzer menu. Heap at the menu moved 80 B between runs (183 176 → 183 096), against up to 2346 B for the old leak. Channel 1 here gives only 0–1.5 frames/s with bursts, so only 3 captures reached the limit. |
| A8 start failure | ❌ H8 | With BLE held, starting a capture hangs the board (see H8). |
| A8 command tables | ✅ | 15 captures: heap at the menu 184 692 → 184 276 B, about 30 B per capture and flat over the last five (was about 800 B). `wifi_refs` stays at 1, `tm` returns to 0. |
| A9 deauth AP | ⚠️ H7 | After exit the driver is down (`svc_up=0`) and the analyzer and Flock get their claims. But the manager AP was already gone before exit (H7), so the exit path had nothing to stop. The rogue-AP and broadcast attacks were not run, because they would target third-party networks. |
| A9 repeat | ✅ | 5 deauth sessions: no duplicate-netif assert. Each scan found 10–15 APs. Heap about 60 B per session. |
| A17 console | ✅ | Chain ssid_spam → deauth → analyzer → captive → HID → trackers → analyzer, with no `exit_app`. Each switch releases the previous app's claims (Wi-Fi 1 → 0, BLE 1 → 0) and the task count returns to baseline. `launch` followed by `sdr_capture`: the analyzer's exit runs (`wifi_svc: deinitialized`), then the designed reboot into SDR mode. |
| A17 no app | ✅ | `launch` from the main menu works. `exit_app` with no app prints "Not currently inside an application." The command is `exit_app`, not `exit` as section 7 says. |
| A12 warthread | ✅ (exit path) | 5 runs, LEFT after 0.3 s, 0.6 s, 8 s, 10 s and 12 s: no panic, the menu is drawn (no stuck `oled_mutex`), and the SD card remounts and a new session file is created each run. Exit restores Wi-Fi to STA (`svc_up=1`), so the task count settles at 9 (the Wi-Fi driver task), stable over 5 runs. CSV content not checked: no GPS fix indoors and no Thread traffic. The GPS screen shows "GPS: 246 sat", a garbage satellite count that predates this work. |
| A13 wardriving | ✅ (exit path) | 5 runs with LEFT inside a blocking scan, matched against the scan's log timestamps. Each exit took 1.4–3.2 s, including console polling. No scan starts afterwards, `wifi_refs=0`, `tm=0`, the task count is back to 8, and the analyzer and deauth work afterwards. Final CSV append not exercised (no GPS fix). |
| A15 UART bridge | ✅ | LEFT queued in the same write as the RIGHT that starts the bridge, 5 times: each time the bridge task starts, sees the exit flag right away and reinstalls the boot UART0 about 1 ms later. Bridge running with data from USB, then a timed LEFT: the console answers afterwards and `uart_bridge_get_config` reports the boot values. Boot is **921600 8N1**, not the 115200 that section 7 expects. Logs-to-UART not checked (no UART adapter). Separately, the bridge's own `uart_driver_install` fails with "already installed" because it never deletes the boot driver first. That predates this work and means the bridge's settings are only partly applied. |
| OTA exit | ✅ | Soft-AP up in APSTA, then LEFT single-click: `svc_up=0` and the 4 leftover tasks are gone (FreeRTOS task count 12 → 8). The laptop stops listing `MININO_AP` within 45 s. A listing 3–25 s after exit is NetworkManager's cache, since the driver is already deinitialized. HTTP server not probed (the laptop did not join the AP). |
| OTA re-entry | ✅ (no update) | 5 enter/exit cycles: no netif or event-handler assert, the AP comes back each time, heap about 80 B per session. A full firmware update was not run (needs a client joined to the AP). |
| OTA then another app | ✅ | After OTA exit, the analyzer (`wifi_refs=1`), Flock (`wifi_refs=1`, `prom_refs=1`) and BLE spam (`ble_refs=1`) each get their claim. |
| 7.3 regression sweep | ✅ counts / ⚠️ heap | One boot: analyzer, deauth, captive, SSID spam, OTA, warthread, wardriving, UART bridge, trackers, Flock, BLE spam. After every exit `wifi_refs`, `ble_refs`, `prom_refs` and `tm` are 0, with no panic, and the menu is drawn. Heap ends about 26 KB below the start (209 164 → 182 688 B with Wi-Fi down). Every part of that is one-time: the AP netif kept for the boot (about 5.8 KB, first deauth), OTA's first run (about 0.8 KB), and the first BLE stack bring-up (about 19 KB, first trackers). Repeated sessions drift only 40–150 B: deauth ×5, OTA ×5, trackers ×4, BLE spam ×3, DroneID scanner ×10. |

### 8.2 Defects found

| ID | Severity | Defect | Status |
|---|---|---|---|
| H7 | High, regression from 726524e2 | Deauth starts the `MININO_MANAGER` AP outside any lease (`wifi_driver_access_point_begin`, APSTA). `wifi_scanner` then claims STA, which switches the mode and drops the soft-AP within about 10 ms, and its release, as the last claim, deinitializes the driver. This happens about 2.5 s after entry, while deauth is still on screen. The laptop never sees the manager AP, and the attacks then run on a driver that isn't initialized. The rogue-AP path reaches `ESP_ERROR_CHECK(esp_wifi_set_mac)` in `wifi_driver_set_ap_mac` ([wifi_controller.c:198](../firmware/components/wifi_controller/wifi_controller.c#L198)), which would abort. The scanner already switched to STA before 726524e2; the new part is the deinit on release. | **Fixed (Phase 10, verified 8.4).** Deauth takes an APSTA `radio_lease` claim for the whole session, so the scanner nests as a second holder and its release no longer deinitialises the driver. |
| H8 | High, from the Phase 9 A8 fix | With BLE held (`gattcmd_scan`), the analyzer's Start hangs the whole board: the console stops answering, and USB-JTAG resets (esptool `default_reset` and `usb_reset`) are ignored. Only unplugging the board recovered it. The log ends at `radio_lease: promiscuous requested while BLE is active`. Probable cause, from the code: `sniffer_start` creates `sniffer_task` (HIGH priority) before claiming promiscuous mode. The task right away runs `sniffer_animation_start_cb` (`oled_screen_clear` plus the animation task) and `sniffer_cb`, and the `err_start` path then force-deletes it with `task_manager_delete(sniffer->task)`, likely while it holds `oled_mutex` (S6). The animation task is never stopped on that path. Before Phase 9 the error was ignored and this path did not run. | **Fixed (Phase 10, verified 8.4).** `sniffer_start` claims promiscuous mode before `task_manager_create(sniffer_task)`, so a failed claim returns before any task is created or draws. The analyzer returns to its menu. |
| H9 | Medium, predates this branch (9c6a53d3) | `rf_spectrum_set_pcap_enabled(true)` tests `if (rf_spectrum_pcap_start())` ([rf_spectrum.c:684](../firmware/main/apps/rf_spectrum/rf_spectrum.c#L684)), but that function returns `esp_err_t`, so success (`ESP_OK` = 0) is treated as failure. The first `on` opens `/sdcard/spectrumNN.pcap` and marks the pcap session active, but `pcap_logging` stays false. Every later `off` returns early (`enabled == pcap_logging`), so the FILE is never closed until reboot, and PCAP logging never records. | **Fixed (Phase 10, verified 8.4).** `if (rf_spectrum_pcap_start() == ESP_OK)`. |

### 8.3 Still pending

- **H4** with the default `Cats1234` password: rebuild with `CONFIG_WIFI_AP_PASSWORD="Cats1234"` (or from a fresh clone).
- **A9 with a target**, **A7**, **OTA update**, **A12/A13 CSV content**, **A15 logs-to-UART**: need, respectively, a target device on an AP you own, a real AP, a client joined to the OTA AP, a GPS fix, and a UART adapter (as in 7.4).
- **A14 forced failure**: needs a temporary hook to fill the task registry or occupy the GPS UART.
- **S3 rest — Modbus TCP + cat_dos `radio_lease` conversion (committed as `c51229bf`, reverted in `825f8f18`; not in HEAD).** The checks below apply once it is re-landed; group it with A16, which needs the same AP. `wifi_ap_manager` now takes one `radio_lease_wifi_acquire(STA)` per session (guarded by `wifi_leased`) and drops it in `wifi_ap_manager_deinit()` with `radio_lease_wifi_release()` instead of `esp_wifi_stop()`. Needs a real AP (or `modbus_sim.py` behind one) and the `radio_state` hook to read `wifi_refs`. This supersedes the A7 hardware test (same equipment). Pending checks, with a clean boot per run:
  - **Lease balance:** enter Modbus TCP (or cat_dos), connect to the AP, exit with LEFT. `wifi_refs` goes 0 → 1 on connect and back to 0 after exit; repeat the connect/exit 5× and confirm it never climbs (no leaked claim, the H1 failure mode).
  - **No teardown under a co-running app:** connect in Modbus TCP, then (via console `launch`, A17) switch to the analyzer or Flock — it must still get its claim, and Modbus TCP's exit must not deinitialise the driver while another holder is up.
  - **BLE refusal path:** start the app while BLE holds the radio (`gattcmd_scan`/trackers). The join must fail cleanly (`WiFi stack init failed`, returns to the menu), with no abort and `wifi_refs` unchanged.
  - **A7 behaviours still hold:** after exit the device is disconnected from the AP, and a later STA disconnect in another app does not trigger reconnects or wipe the saved `ssid`/`passwd`.
  - **Driver left down, not stopped-under-others:** with no other Wi-Fi holder, exit releases the last claim and the driver deinitialises (`svc_up=0`), same as the other converted apps (6.3).

  **Why it was reverted (investigated 2026-11-27).** No reason is recorded: `825f8f18` carries the plain default `git revert` message, the revert is clean and total (HEAD is back to `initialized` + `esp_wifi_stop()`, `radio_lease` dropped from the `CMakeLists.txt` REQUIRES), and the conversion was never re-landed in any form. Timeline: committed 2026-11-26 16:02, reverted 16:46 — 44 min later, same author, the same day as the section 8 hardware round. A line-by-line read of the `c51229bf` diff found **no concrete defect**: it built, and it is semantically correct (the "ap manager" is a STA client that joins an external AP — used by Modbus TCP, cat_dos, Wi-Fi settings, cmd_wifi — so `RADIO_WIFI_MODE_STA` is right, and `join()` sets STA mode explicitly before `esp_wifi_connect()` regardless of how the driver was brought up). Best-evidence conclusion (inference, not a recorded fact): it was backed out because it was **the only change in that session that could not be verified on hardware** — it needs a real AP (see above) — and the author kept the committed branch to hardware-tested changes only, intending to re-land it after testing. The default revert message fits "remove to re-land later", not "found a bug". **One edge case to cover when it is re-landed** (it is already the "No teardown under a co-running app" check above): if another Wi-Fi app holds the driver first, `radio_lease` keeps the first holder's mode, so Modbus would run in NULL/promiscuous mode where `esp_wifi_connect()` could fail — harmless under A17 (one app at a time from the menu), but worth asserting in the test.

### 8.4 Phase 10 fixes, verified on hardware (2026-11-26)

Built and flashed clean (the section 8 test hooks were removed first). Each fix was checked on the board, then a clean build with no hooks passed an enter/exit sweep of deauth, analyzer, rf_spectrum and the BLE-held analyzer start.

- **H9 (`rf_spectrum.c:684`, `== ESP_OK`).** `spectrum_pcap on` now reports RECORDING and the file grows (`/sdcard/spectrumNN.pcap`, 128 and 118 packets saved over two runs); `off` closes it ("PCAP recording stopped"), and a later `on` opens a fresh file. This also covers **A11**: toggling off while frames arrive, repeated, caused no crash. The driver bring-up still has no lease, so it stays started after exit (unchanged, 6.3).
- **H8 (`cmd_sniffer.c`, claim before task).** With `gattcmd_scan` holding BLE, `launch analyzer` then start: the claim is refused at `radio_lease: promiscuous requested while BLE is active` **before** the task is created (`tm=0`), the console stays responsive, and the analyzer stays on its menu (`in_app=1`, menu 12). LEFT exits to the parent menu. No hang, no reset needed.
- **H7 (deauth APSTA lease).** `MININO_MANAGER` (WPA2, ch 1) comes up on entry and stays up for the whole session — the log shows no `wifi_svc: deinitialized` between the AP start and the LEFT press, where before the scanner tore it down about 2.5 s in. `wifi_refs` stays 1 during the session. On exit the driver is deinitialised (`svc_up=0`), so the AP stops; a scan a few seconds later can still show it from the laptop's NetworkManager cache (same caveat as OTA in 8.1). The analyzer and Flock get their claims afterwards. The rogue-AP abort path (`esp_wifi_set_mac`) is removed with it, but was not re-tested on hardware (it needs a cloned SSID).

**Tree state after Phase 10:** the temporary `radio_state`, `press_after` and `loglvl` console commands are removed; `cmd_control.c` carries only the A17 change. `PCAP_FLASH_MAX_PACKETS` is back to 900. Files touched: `main/apps/rf_spectrum/rf_spectrum.c`, `components/wifi_sniffer/cmd_sniffer.c`, `components/wifi_controller/wifi_controller.c`, `main/apps/wifi/deauth/deauth_module.c`.

## 9. Hardware verification round of 2026-11-26 (committed tree + re-added hook)

Runs the Phase 8/9 pending items from section 7 that do not need extra equipment, against the **committed** branch (HEAD `c51229bf`) with the temporary `radio_state` console command re-added (and nothing else).

- **Device:** the same Minino (ESP32-C6) on `/dev/ttyACM0`, ESP-IDF v5.5.1.
- **Build under test:** committed tree + a re-added `radio_state` hook in `main/modules/cmd_control/cmd_control.c` (registered in `main/modules/cat_dos/cat_console.c`). It prints `wifi_refs ble_refs prom_refs svc_up mode ch tm(=task_manager count) tasks(=uxTaskGetNumberOfTasks) heap` from the existing public getters. `press_after`/`loglvl` were **not** re-added — host-side `time.sleep` plus the existing `press` command covered timed input. This hook is **uncommitted and must be removed before committing**, like the section 8 hooks.
- **Harness:** a throwaway pyserial driver (not added to the repo). Opening the USB-JTAG port asserts DTR/RTS and resets the board, so every group starts from a clean boot. `general_submenu` only acts on `BUTTON_PRESS_DOWN`, so in-app selection uses `press right`/`press left` with the default (press_down) event.
- **A run fails on any panic, abort, assert, reboot, or a console that stops answering.** None occurred.
- **Scope decision:** only the console-automatable, non-emitting tests plus OTA were run. **The deauth rogue-AP/broadcast and SSID/BLE-spam _transmit_ paths were deliberately not started** (they flood third-party RF), matching the audit's own refusal in 8.1; those apps were exercised on their enter/exit lifecycle only.

**Baseline at rest (idle menu, no app):** `wifi_refs=0 ble_refs=0 prom_refs=0 svc_up=1 mode=2 ch=1 tm=0 tasks=8 heap≈188 700`. `mode=2` (AP) with `svc_up=1` is the known harmless boot quirk (6.3).

### 9.1 Results

| ID (section) | Result | What the board showed |
|---|---|---|
| A17 console (7.1) | ✅ | Chain `ssid_spam → deauth → analyzer → captive → hid → trackers → analyzer` with no `exit_app`. Each launch released the previous app's claims — clearest: trackers (`ble=1 tm=1 tasks=15`) → analyzer (`ble=0 tm=0 tasks=9`). Final `exit_app` → all counts 0, `tasks=8`. No app stacked. No attack started. |
| A17 no app (7.1) | ✅ | `launch analyzer` from the main menu works; `exit_app` with no app prints "Not currently inside an application." |
| A10 ×10 (7.2) | ✅ | 10 enter/exit cycles: `prom 1→0`, `tasks 10→8` each time, heap `186 116 → 185 480` then flat. No crash. |
| A10 BLE-held (7.2) | ✅ | `gattcmd_scan` holds BLE; `launch droneid_scanner` → `WiFi bring-up vetoed: BLE owns the radio` / `WiFi stack init failed`, no tasks start (`tasks=11` unchanged), `in_app=0`. Lease guard refuses before the promiscuous claim. |
| A8 start failure / H8 (7.1) | ✅ | BLE held (`gattcmd_scan`), `launch analyzer`, press Start: logs `promiscuous requested while BLE is active` + `WiFi requested while BLE is active`, **no task created** (`tm=0 prom=0`), console stays responsive, analyzer stays on its drawn menu (`in_app=1 menu 12 ink=148`). **No board hang** — the old H8 failure is gone. |
| A8 command tables (7.1) | ✅ | 12 capture cycles (run cut at 12 by the host timeout; trend decisive). Each: capturing `prom=1 tm=1`; at main `wifi=1 tm=0 tasks=8`. Heap@main `184 720 → 184 288`, ≈39 B/cap and flat over the last six — far below the old ~800 B. |
| A11 / H9 (7.2) | ✅ | `launch spectrum` (`prom=1 mode=1 tasks=11`); `spectrum_pcap on` → "PCAP logging enabled / STARTED"; `off` → "STOPPED"; `on` → fresh file. 10 rapid toggles: no crash, `prom=1 tasks=11` stable. Exit → `prom=0`; driver stays started (`svc_up=1 mode=1 tasks=9`), the documented no-lease behaviour (6.3). |
| A14 normal path (7.2) | ✅ | trackers; DOWN toggles the session. `tm` cycles `0→1→0→1→0` (never climbs — no GPS task-registry leak), `tasks` and heap fully recover each restart (`188 500` / `187 816`). start/stop/start works. Forced-failure variant needs a registry-fill hook (8.3), not run. |
| OTA exit (7.1) | ✅ | Enter `mode=3` (APSTA) `tasks=12 ink=446`; LEFT single-click → `http_server_stop: stopping HTTP Server` / monitor / `wifi_svc: deinitialized`; `svc_up=0`, `tasks 12→8` (the 4 leftover tasks gone), heap `204 696`. |
| OTA re-entry (7.1) | ✅ | 5 enter/exit cycles: AP back each time (`mode=3`), no netif/event-handler assert, heap ≈71 B/cycle. |
| OTA then another app (7.1) | ✅ (partial) | After OTA exit: analyzer `wifi_refs=1`; Flock `wifi_refs=1 prom_refs=1`. **BLE-spam-after-OTA not confirmed** — BLE spam claims the BLE lease only on Start (an RF transmit, not run); at its menu it holds no claim (`ble=0`). |

### 9.2 Not run (unchanged from 7.4 / 8.3)

Need equipment, an environment, or a code hook: **A7 / Modbus TCP + cat_dos** (real AP; 8.3), **A9 rogue-AP/broadcast and A9-with-target** (RF attacks on third-party networks — deliberately not run), **OTA firmware upload** (a client on the AP), **A12 / A13 CSV content** (GPS fix), **A15 logs-to-UART** (UART adapter), **H4 `Cats1234` default** (fresh-clone build; the open-AP fallback is covered in 8.1), **A14 forced failure** and **A8 flash limit** (registry-fill / sdkconfig-limit hooks; flash limit was out of this round's agreed scope).

**Firmware state after this round:** the temporary `radio_state` hook has been reverted (`git checkout` on `main/modules/cmd_control/cmd_control.c`, `.h` and `main/modules/cat_dos/cat_console.c` — those three files now match HEAD `c51229bf`, no `radio_state` references remain). The board was rebuilt and reflashed with the **clean committed build** (app only, SPIFFS preserved) and boots to its menu; `radio_state` on the console now returns "Command not found". The working tree carries no uncommitted source changes from this round (only this audit file is edited). ESP-IDF v5.5.1.

## 10. Hardware verification of S6 + the SD-unmount handler (2026-11-27)

Tests the work committed since the audit that had not run on a board: the S6
cooperative-stop fixes (`2a8cc57d`..`347ced73`) and the SD-unmount shutdown
handler (`c0f2b880`).

- **Device:** the same Minino (ESP32-C6) on `/dev/ttyACM0`, ESP-IDF v5.5.1.
- **Build under test:** committed tree (HEAD `c0f2b880`) + a re-added `radio_state` console hook (prints `wifi_refs ble_refs prom_refs svc_up mode ch tm tasks heap in_app` from the public getters). For the SD test only, a second temporary marker `esp_rom_printf("SHUTDOWN_SD_UNMOUNT rc=%d")` was added inside the shutdown handler, since buffered `ESP_LOG` is lost in the USB-JTAG teardown on restart. **Both hooks are uncommitted and have been reverted; the board was reflashed with the clean committed build (`radio_state` → "Command not found").**
- **Harness:** a throwaway pyserial driver (not in the repo). Opening the USB-JTAG port and pulsing DTR/RTS resets the board, so every group starts from a clean boot. In-app selection uses `press right`/`press left` (the submenu acts on press-down).
- **A run fails on any panic, abort, assert, reboot, or a console that stops answering.** None occurred.

**Baseline at rest:** `wifi_refs=0 ble_refs=0 prom_refs=0 svc_up=1 mode=2 ch=0 tm=0 tasks=8 heap≈188 600 in_app=0` (the `mode=2`/`svc_up=1` boot quirk, 6.3).

### 10.1 S6 results

Pass = no panic/abort/reboot, no `"did not stop"` force-delete log, and after exit `wifi_refs`/`ble_refs`/`prom_refs`/`tm` are 0 with `tasks` back to baseline.

| App (S6 site) | How the task was exercised | Result |
|---|---|---|
| deauth scan task | `launch deauth` auto-starts the scan; `exit_app` presses LEFT mid-scan. ×3, plus LEFT at 0.3 / 0.6 / 2 / 5 / 8 s into the scan | ✅ IN `wifi=2 tasks=11`; OUT every time `tasks=8 wifi=0 prom=0 tm=0`, `svc_up=0`. ~50 B/cycle heap drift |
| DroneID scanner (channel + process tasks) | `launch droneid_scanner`, exit. ×3 | ✅ IN `prom=1 tasks=11`; OUT `prom=0 tm=0 tasks=9` (driver left resident in NULL, 6.3), stable. ~77 B/cycle |
| `ieee_sniffer` debug task | `launch thread_sniffer`, exit. ×3 | ✅ IN `tasks=11`; OUT `tm=0 tasks=9`, stable. ~75 B/cycle |
| wardriving scan task + animation task | `launch wardriving_start` auto-starts a blocking scan and the flag-driven animation; exit mid-scan. ×3 | ✅ IN `wifi=1 tasks=12`; OUT `tasks=8 wifi=0 tm=0`. ~43 B/cycle |
| deauth detector channel hopper | `launch deauth_scan` → `press right` (Run) starts hopping; exit mid-hop. ×2 | ✅ IN `prom=1 wifi=1` with `ch` advancing (3→4); OUT `prom=0 wifi=0 tm=0 tasks=8` |
| captive scan task | `launch captive`, enter/exit lifecycle. ×3 | ✅ exit path clean (`tasks=8`, refs 0). The scan task itself needs the FS-portal Run path (deeper menu nav); not driven here |
| odrone spoofing task (drone_id) | `launch drone_id`, enter/exit lifecycle. ×3 | ✅ exit path clean (`tasks=9`, refs 0). The spoofing task starts on the Run action (menu nav); not driven here |

No `"did not stop"` log appeared in any run, i.e. every task left on its stop flag within the timeout and the force-delete fallback was never reached.

### 10.2 SD-unmount shutdown handler

`sd_card_begin()` does not mount the card at boot and silences the `sd_card`
log tag, and late `ESP_LOG` output is lost in the USB-JTAG teardown, so the
handler was observed through the temporary `esp_rom_printf` marker instead.

- Console `reset` (which calls `esp_restart()`) at idle, and again after
  entering an app: the marker printed **`SHUTDOWN_SD_UNMOUNT rc=0`** both times
  and the board rebooted normally (`rst:0xc SW_CPU`). The handler runs on the
  restart path and `sd_card_unmount()` returns `ESP_OK` (it is idempotent:
  `ESP_OK` whether or not the card was mounted). No hang, no crash on the
  restart.
- This confirms the fix: deliberate restarts now pass through the handler. The
  FAT "clean on restart" effect itself (card contents) was not read back —
  that needs pulling the SD card — but the audited gap was that the handler did
  not exist/run, and it now does.

### 10.3 Not run this round

- **captive** FS-portal scan and **drone_id** odrone spoofing *under load*: only the enter/exit lifecycle was driven from the console; starting those tasks needs deeper menu navigation.
- **cat_dos/A16** (fixed and hardware-verified with a deterministic target, section 12; the real-AP checks in 8.3 are still blocked on a Wi-Fi join that timed out), **A18** (BLE spam, since committed), and everything in 7.4 / 8.3 / 9.2 that needs equipment or an environment.

**Firmware state after this round:** both temporary hooks (`radio_state` in `cmd_control.*`/`cat_console.c`, and the `esp_rom_printf` marker in `main/main.c`) were reverted with `git checkout`; the working tree has no uncommitted source changes (only this audit file). The board was rebuilt and reflashed with the clean committed build (app only, SPIFFS preserved) and boots to its menu; `radio_state` returns "Command not found". ESP-IDF v5.5.1.

## 11. Hardware verification of A18 (2026-11-27)

Tests the four A18 fixes (uncommitted tree, on top of HEAD `c0f2b880`).

- **Device:** the same Minino (ESP32-C6) on `/dev/ttyACM0`, ESP-IDF v5.5.1.
- **Build under test:** the tree + a temporary `radio_state` console hook (adds `sa` = `radio_lease_ble_scan_allowed()`, `tm` = `text_mutex != NULL` and the presence of the sniffer tasks), plus temporary `scanblock`/`scanunblock` commands to hold a second scan block. For one run, an `esp_rom_printf` in `bt_gattc_task_stop` timed the wait for `UNREG_EVT` (the `ESP_LOGW` there is silenced by `ESP_LOG_NONE` without `CONFIG_BT_GATTC_DEBUG`). All of it was reverted; the board was reflashed with the clean tree (`radio_state` → "Command not found").
- **Harness:** throwaway pyserial driver in the scratchpad (not in the repo), as in section 10.
- **Authorization:** BLE spam transmits. The user allowed short bursts (about 2 s, 8 total) in a controlled environment.
- **A run fails on any panic, abort, assert, reboot, or a console that stops answering.** None occurred.

**Baseline:** `wifi=0 ble=0 prom=0 tasks=8 heap≈188 600 sa=1 tm=0`, no sniffer task alive.

| Item | Result | What the board showed |
|---|---|---|
| A18 #1 BLE spam double stop | ✅ | LEFT runs `ble_spam_stop` and then the menu's `on_exit_cb` (also `ble_spam_stop`). 5 bursts: `ble` 1→0, `sa` 0→1, `tasks` 12→7. With a second scan-block holder (`scanblock`) present during spam, `sa` stayed 0 after exit and went to 1 only after `scanunblock`. Before the fix, the second unblock would have dropped the holder's block. `exit_app` path ×3: same. |
| A18 #2 `text_mutex` | ⚠️ not exercisable | `tm` stays 1 after exit on every path. `launch spam_*` goes through the menu entries, whose `on_exit_cb` is `ble_spam_stop`; `ble_spam_on_exit` is never called (only the unused `ble_spam_app` wrapper reaches it). The fix is verified by build and reading only, and stays latent as the audit said. |
| A18 #3 `bt_gattc_task_stop` | ✅ | trackers ×5 and adv_scanner (2 real start/stop cycles, the others did not reach Start because the menu remembers its last option): `ble` 1→0, `tasks` 13→6 and 10→6, no panic. The `UNREG_EVT` was already set when the wait began (`unreg_wait_ms=0 done=1`, 5/5), so the BTC task runs the unregister before `esp_ble_gattc_app_unregister` returns on this build; the wait costs nothing here and covers builds where it does not. |
| A18 #4 `ieee_sniffer_arm` | ✅ normal path / ⚠️ race not forced | zigbee_sniffer, zbmesh, thread_sniffer, warbee, warthread ×3 each: the sniffer task is alive inside, gone after exit, `tasks` back to its post-app level (7–9), heap drift under 100 B per cycle. The real race (a stop in the few ms between `xTaskCreate` and the task's first slice) cannot be hit from the console. |

**Finding (not from A18, predates it):** `launch zigbee_sniffer` followed by `exit_app` within about 100 ms leaves `zb_sniff_task` and the debug task running, and `exit_app` then reports "Not currently inside an application". `zigbee_module_sniffer_exit()` returns at once because `sniffer_running` is only set at the end of `zigbee_module_sniffer_enter()`, after `wifi_service_deinit()`. A console `exit_app` that lands mid-entry therefore stops nothing and the entry then completes. Warbee and warthread also stayed up with `exit_app` at 0.8 s because their entry is slow. A physical button cannot do this, since the menu is busy during entry; only the console `launch`/`exit_app` pair can. Recovered with a board reset. Not fixed.

**Firmware state after this round:** the hooks were reverted (`git checkout` on `cmd_control.c`, the printf removed from `bt_gattc.c`); the tree carries only the A18 changes. ESP-IDF v5.5.1.

## 12. Hardware verification of A16 (2026-11-27)

Tests the A16 fix (uncommitted tree, on top of HEAD `96863d14`).

- **Device:** the same Minino (ESP32-C6) on `/dev/ttyACM0`, ESP-IDF v5.5.1.
- **Build under test:** the tree + a temporary `radio_state` console hook in `cmd_control.c`/`cat_console.c` (prints `wifi_refs ble_refs prom_refs svc_up mode ch tm tasks heap in_app`). Reverted after the round; the board was reflashed with the clean tree (`radio_state` → "Command not found").
- **Harness:** a throwaway pyserial driver in the scratchpad (not in the repo), as in sections 10–11.
- **AP used:** the user offered their own phone hotspot ("S24+ de Daniel") for a live-connection test. Both `connect` to that network and to the already-saved `EC_HQ` **timed out** in `wifi_ap_manager_connect_ap` (no association). This is unrelated to A16 — `wifi_ap_manager`'s join path was not touched by this fix — and most likely a band mismatch: the ESP32-C6 radio is 2.4 GHz-only, and a phone hotspot often defaults to 5 GHz or auto. **Not fixed, not investigated further**; retry with the hotspot's band forced to 2.4 GHz-only if this needs to be re-run. Lacking a working join, the test below uses a deterministic target (`192.0.2.1`, the RFC 5737 documentation/test range) instead of a real server — sufficient for A16, since the bug is in the pthread exit path itself, not in whether the target responds.
- **A run fails on any panic, abort, assert, reboot, or a console that stops answering.** None occurred.

**Baseline:** `wifi_refs=0 ble_refs=0 prom_refs=0 svc_up=1 mode=2 ch=1 tm=0 tasks=8 heap≈188 600 in_app=0` (the `mode=2`/`svc_up=1` boot quirk, 6.3).

| Check | Result | What the board showed |
|---|---|---|
| Thread lifecycle, 8 cycles | ✅ | `launch dos` → `web_config 192.0.2.1 80 /` → `catdos` (console attack, bypasses the menu's "connect first" gate) → 1.2 s running → `exit_app`. Each cycle: entering `tasks=7→16` (`tm=1`, the 8 pthreads + `catdos_send`), `heap` drops ~46 KB while attacking; `exit_app` returns a real **~0.25 s** after the command (measured by polling for the prompt, not a fixed sleep) on every one of 8 cycles, `tasks` back to 7, `tm` back to 0, no `"Attack task did not stop"` `ESP_LOGE` on any cycle — `task_manager_wait_exit()` never needed its 5 s fallback. No panic, no stuck console. |
| Sustained loop, not just one shot | ✅ | With no live network, `getaddrinfo("192.0.2.1", ...)` itself fails (`err=201`) and the DNS-failure branch `continue`s with a 1 s delay, so the 8 pthreads loop continuously on `running_attack` rather than exiting after one attempt — this is exactly the "stop a running loop" case the fix targets, and `exit_app` (1.2 s into that loop) still returned clean and fast every time. |
| First attempt, methodology note | — | An earlier pass called `exit_app` before `catdos_on_exit`'s 2 s BETA splash cleared, so `menus_module_get_app_state()` was still false; `exit_app` printed "Not currently inside an application" and the attack task was never told to stop, which piled up tasks/heap across cycles — a test-harness bug (not waiting for `in_app`), not an A16 regression. Recovered with a board reset; the corrected run above waits for the splash. |

**Firmware state after this round:** the `radio_state` hook was reverted (`git checkout` on `cmd_control.c`/`cmd_control.h`/`cat_console.c`); the tree carries only the A16 fix. ESP-IDF v5.5.1.
