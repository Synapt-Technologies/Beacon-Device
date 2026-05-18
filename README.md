# Beacon-DIY
Firmware written for DIY devices in the Beacon ecosystem


# TODO
 - [ ] Section grouping
 - [ ] Multi alert text indeces
 - [ ] Make textAlert an array of tasks in the group, one per index of alert text.
 - [ ] Architecture refactor.
   - [ ] connection -> producer
 - [ ] On ILvgl every tally updates means font recalc. Inefficient.


# DEVNOTES

## Alerts

### ColorAlert handeling:
Only target. ConsumerGroup starts task, calls setAlertStep on each consumer

### TextAlert handeling:
Each consumer stores a map between alertIndex and textIndex. The group starts the task of the index and target and calls setAlertText on the consumer.