# SMS forwarding contract

The rotary phone forwards every SMS its SIM receives to Home Assistant (HA). HA shows
it on the Pixel 8 only and acknowledges it to the phone after the Pixel reports the
notification received. The phone deletes the message from modem storage only after
that acknowledgement. `tsuryphone_sms.yaml` in this folder is the HA side.

## Phone to HA: delivery

`POST <HA base URL>/api/webhook/<TSURYPHONE_SMS_WEBHOOK_ID>` with
`Content-Type: application/json`. The HA base URL is the one already set on the phone
(`POST /api/config/ha_url`). The webhook is `local_only`, so the phone needs no HA
account token.

```json
{
  "message_id": "m1f3a9c0d22b47e15",
  "revision": "r5ce0fc34",
  "sender": "+972500000001",
  "received_at": "2026-11-24T10:15:04+02:00",
  "text": "Your WhatsApp code: 123-456",
  "parts": {"received": 2, "total": 2, "complete": true, "missing": []}
}
```

- `message_id` is stable for a message, including all parts of a multi-part
  message. HA uses it as the notification tag, so a newer revision replaces the
  older notification.
- `revision` changes whenever the payload changes, for example a partial message
  and then the complete one.
- IDs start with `m` and revisions with `r`, so HA templates keep them as strings.
- `sender` is the number in international form, or the alphanumeric sender name.
- `received_at` is the network time stamp of the first part.
- `text` is UTF-8. GSM 7-bit, 8-bit and UCS2 (Hebrew) messages are decoded, and
  multi-part messages are joined.
- A multi-part message is posted once it is complete. If parts are still missing
  10 minutes after the first part arrived, the received parts are posted with
  `complete: false`, the missing part numbers, and a `[part N of M missing]`
  marker in the text. When the rest arrives, the complete text is posted as a new
  revision.

The phone re-posts the current revision until it is acknowledged. The wait
between posts doubles from 5 seconds and is capped at 1 minute. When Wi-Fi comes
back, every waiting message is posted at once. Messages stay in modem storage,
so restarts and Wi-Fi loss lose nothing. The phone also lists storage at boot,
after Wi-Fi recovery, after a modem reset, and every 10 minutes.

## HA to phone: acknowledgement

`POST http://<phone address>:8080/api/sms/ack` with header
`X-TsuryPhone-Ack-Key: <TSURYPHONE_SMS_ACK_KEY>` and body
`{"message_id": "...", "revision": "..."}`.

| Status | Meaning |
| --- | --- |
| 200 | Accepted. For the complete revision, the phone deletes every stored part. For a partial revision, it stops re-posting that partial and keeps the parts. |
| 401 | Missing or wrong ack key. |
| 400 | Missing `message_id` or `revision`. |
| 404 | Unknown message (already deleted, or never received). |
| 409 | Stale revision: the payload changed since that revision. Nothing changes. |

## HA automation

`tsuryphone_sms.yaml` holds the `rest_command` for the acknowledgement and the
automation:

- Webhook trigger, `local_only: true`, POST only; mode `parallel`;
  `trace: stored_traces: 0`.
- Notifies `notify.mobile_app_pixel_8` only, never `notify.our_phones`, with
  `tag` = message ID and `confirmation: true`.
- Waits up to 2 minutes for `mobile_app_notification_received` with the Pixel's
  `device_id` and `tag` = that message ID. No acknowledgement on timeout; the
  phone posts again.
- Then acknowledges the revision it notified.

`mobile_app_notification_received` fields, observed on the live Pixel 8 on
2026-11-24: `message`, `title`, `tag`, `channel`, `confirmation`, `webhook_id` and
`device_id`. The push relay keeps only the keys it knows, so custom data keys such
as a message ID or revision never reach the event. That `device_id` is the
Companion app's own device ID from its registration, not the HA device registry
ID. The app sends the event only when `confirmation: true` is set.

The match has no revision, so one received event releases every waiting run for
that message. The phone answers 409 for a stale revision. If the received event of
a partial revision arrives after the complete revision was posted, the complete
revision is acknowledged before the Pixel confirms it; its notification has the
same tag and normally arrives within seconds.

## Live proof: Wi-Fi suspend

`POST http://<phone address>:8080/api/wifi/suspend` with body `{"seconds": 300}`
and the OTA password as HTTP Basic auth (any user name, the same check as the
firmware upload) disconnects the phone's Wi-Fi about half a second after the
reply. The phone reconnects by itself after `seconds` (1 to 300), even if the
caller is gone; a restart also reconnects.

| Status | Meaning |
| --- | --- |
| 202 | Suspension scheduled. |
| 401 | Wrong or missing password. |
| 400 | `seconds` missing or outside 1-300. |
| 409 | A call is ringing, dialing, active or waiting, the ringer is on, or a suspension is already running. |

## Install (Tsury/TsuryPhone#9)

1. Add these keys to HA `secrets.yaml`:
   - `tsuryphone_sms_webhook_id`: the `TSURYPHONE_SMS_WEBHOOK_ID` value from the
     1Password agent vault.
   - `tsuryphone_sms_ack_key`: the `TSURYPHONE_SMS_ACK_KEY` value from the vault.
   - `tsuryphone_sms_ack_url`: `http://<phone address>:8080/api/sms/ack`.
   - `tsuryphone_sms_pixel_app_device_id`: the Pixel 8 Companion app device ID,
     for example from the `device_id` of a test `mobile_app_notification_received`
     event, or the Pixel's `mobile_app` config entry data.
2. Install `tsuryphone_sms.yaml` as a package and restart HA, or reload
   automations and REST commands.
3. Store the webhook ID and ack key as the password of the vault items
   `TsuryPhone SMS webhook ID` and `TsuryPhone SMS ack key`, or name other items in
   `TSURYPHONE_SMS_WEBHOOK_ITEM` and `TSURYPHONE_SMS_ACK_ITEM` in the private values
   file. `scripts/update_phone.py` reads them when the environment lacks them.

Nothing here depends on the dev machine.

## Limits

- Plain HTTP on the home network, like the phone's other API routes and its HA
  link.
- HA keeps notification text in its normal local history (accepted in planning
  grill Tsury/TsuryPhone#4, question 1).
- HA turns a template result that reads as a plain number into a number. A message
  whose whole text is a decimal such as `12.50` shows as `12.5`.
