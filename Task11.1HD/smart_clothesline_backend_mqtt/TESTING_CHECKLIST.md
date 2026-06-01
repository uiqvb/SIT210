# Smart Clothesline Testing Checklist

## A. Line-following accuracy
- [ ] Upload the modified Nano sketch.
- [ ] Place the robot on the middle guide line.
- [ ] Run `RETURN_TO_DRYING` from the dashboard or serial `d`.
- [ ] Confirm the robot follows the middle line without immediately jumping into full line hunting.
- [ ] Briefly lift/misalign the robot or cover the middle line for less than 700 ms.
- [ ] Expected: event becomes `LINE_LOST_GRACE_FORWARD` and robot continues/reacquires line.
- [ ] Cover/remove the middle line for around 1-2 seconds.
- [ ] Expected: event becomes `SOFT_LINE_RECOVERY_ACTIVE` and robot gently corrects using last known direction.
- [ ] Keep the middle line missing for more than about 2.7 seconds total.
- [ ] Expected: robot stops, event becomes `LINE_LOST_FAULT_STOPPED`, dashboard line fault shows YES.

## B. Movement timeout
- [ ] Command `MOVE_TO_SAFE` or `RETURN_TO_DRYING`.
- [ ] Physically block the robot or prevent it from reaching the target.
- [ ] Wait 30 seconds.
- [ ] Expected: robot stops, event becomes `MOVE_TO_ZONE_TIMEOUT_STOPPED`, dashboard move timeout shows YES.
- [ ] Start a movement when the robot cannot find the line at all.
- [ ] Wait 10 seconds.
- [ ] Expected: robot stops, event becomes `LINE_HUNT_TIMEOUT_STOPPED`.

## C. BH1750 light robustness
- [ ] Put the robot in `DRYING` state.
- [ ] Shine light evenly on both BH1750 sensors.
- [ ] Expected: event becomes `SUN_BALANCED_LOCKED`; robot stops rotating for about 60 seconds.
- [ ] Shine light more strongly on one side.
- [ ] Expected: robot rotates in short steps toward the brighter side.
- [ ] Keep one BH1750 covered for around 10 seconds.
- [ ] Expected: event becomes `LIGHT_DEGRADED_LOCKED`; dashboard light mode shows DEGRADED; robot stops endless balancing.
- [ ] Keep the sensors unbalanced but not fully covered for more than 10 seconds.
- [ ] Expected: event becomes `LIGHT_TRACKING_TIMEOUT_LOCKED`; robot locks instead of rotating forever.

## D. Weather location configurability
- [ ] Start the backend and open the dashboard.
- [ ] Enter a location name, latitude, longitude, and timezone in the Weather Backup card.
- [ ] Click `Save Location`.
- [ ] Expected: dashboard Active location updates; event log shows `WEATHER_LOCATION_UPDATED`.
- [ ] Click `Refresh Weather`.
- [ ] Expected: weather values are retrieved for the configured location, or a clear API error is shown.
- [ ] Restart the backend.
- [ ] Expected: saved location persists because it is stored in `data/weather_config.json`.

## E. Rain/weather fallback
- [ ] Confirm normal rain sensor telemetry appears on dashboard.
- [ ] Simulate invalid/missing rain telemetry if possible.
- [ ] Expected: backend uses weather fallback.
- [ ] If weather API is unavailable while rain sensor is faulty, expected fallback reason: `RAIN_SENSOR_FAULT_WEATHER_UNAVAILABLE_DEFAULT_SAFE` and command recommendation `MOVE_TO_SAFE` unless robot is already safe.

## F. Suggested evaluation table
| Criterion | Test | Metric |
|---|---|---|
| Navigation accuracy | 10 safe-to-drying and 10 drying-to-safe runs | direct success rate, line-loss count |
| Robustness | line removed, one BH1750 covered, robot blocked | correct fault state triggered |
| Responsiveness | line loss to recovery/fault, light imbalance to lock, command to target | measured seconds |
| Configurability | change weather location through dashboard | save success and persistence |
