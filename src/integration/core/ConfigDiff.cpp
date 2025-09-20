#include "ConfigDiff.h"

bool buildDndConfigDelta(IntegrationService &svc,
                         const DndConfig &prev,
                         const DndConfig &curr,
                         JsonDocument &outDoc) {
  ConfigDeltaAggregator agg(svc);
  agg.addIfChanged("dnd.force", prev.force, curr.force);
  agg.addIfChanged("dnd.schedule", prev.scheduled, curr.scheduled);
  agg.addIfChanged("dnd.startMinute", (int)prev.startMinute, (int)curr.startMinute);
  agg.addIfChanged("dnd.endMinute", (int)prev.endMinute, (int)curr.endMinute);
  agg.addIfChanged("dnd.startHour", (int)prev.startHour, (int)curr.startHour);
  agg.addIfChanged("dnd.endHour", (int)prev.endHour, (int)curr.endHour);
  bool prevEnabled = prev.force || prev.scheduled;
  bool currEnabled = curr.force || curr.scheduled;
  if (prevEnabled != currEnabled) {
    agg.add("dnd.enabled", prevEnabled, currEnabled);
  }
  if (agg.hasChanges) {
    outDoc = std::move(agg.doc);
  }
  return agg.hasChanges;
}

bool buildAudioConfigDelta(IntegrationService &svc,
                           const AudioConfig &prev,
                           const AudioConfig &curr,
                           JsonDocument &outDoc) {
  ConfigDeltaAggregator agg(svc);
  agg.addIfChanged("audio.earpieceVolume", (int)prev.earpieceVolume, (int)curr.earpieceVolume);
  agg.addIfChanged("audio.earpieceGain", (int)prev.earpieceGain, (int)curr.earpieceGain);
  agg.addIfChanged("audio.speakerVolume", (int)prev.speakerVolume, (int)curr.speakerVolume);
  agg.addIfChanged("audio.speakerGain", (int)prev.speakerGain, (int)curr.speakerGain);
  if (agg.hasChanges) {
    outDoc = std::move(agg.doc);
  }
  return agg.hasChanges;
}
