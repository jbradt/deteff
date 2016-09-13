from deteff.trigger import TriggerSimulator
from atmc.mixins import TrackerMixin, EventGeneratorMixin
import csv

__all__ = ['TriggerSimulator', 'TriggerEfficiencySimulator']


def read_padmap(path):
    padmap = {}
    with open(path, 'r') as f:
        reader = csv.reader(f)
        for row in reader:
            row = [int(v) for v in row]
            padmap[row[-1]] = tuple(row[:-1])
    return padmap


class TriggerEfficiencySimulator(EventGeneratorMixin, TrackerMixin):
    def __init__(self, config):
        super().__init__(config)

        self.padmap = read_padmap(config['padmap_path'])

        self.trigger = TriggerSimulator(self.tracker, self.evtgen, config, self.padmap)

    def run_track(self, params):
        return self.trigger.did_trigger(params)
