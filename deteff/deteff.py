from .trigger import TriggerSimulator
from pytpc.fitting.mixins import TrackerMixin, EventGeneratorMixin
from pytpc.relativity import find_proton_params
from pytpc.utilities import find_vertex_energy
from pytpc.constants import pi, p_mc2
import numpy as np
import pandas as pd
import csv
import logging

__all__ = ['TriggerSimulator', 'TriggerEfficiencySimulator', 'read_padmap', 'make_params']

logger = logging.getLogger(__name__)


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

        self.trigger = TriggerSimulator(config, self.padmap)

    def run_track(self, params):
        tr = self.tracker.track_particle(*params)
        evt = self.evtgen.make_event(tr[:, :3], tr[:, 4])
        trig = self.trigger.find_trigger_signals(evt)
        mult = self.trigger.find_multiplicity_signals(trig)
        did_trig = self.trigger.did_trigger(mult)

        result = {
            'num_pads_hit': len(evt),  # evt is a dict of traces indexed by pad number
            'trig': did_trig,
        }

        return result


def make_params(beam_enu0, beam_mass, beam_chg, proj_mass, proj_chg, gas, num_evts):
    params = pd.DataFrame(0, columns=('x0', 'y0', 'z0', 'enu0', 'azi0', 'pol0'), index=range(num_evts))
    params.z0 = np.random.uniform(0, 1, size=num_evts)
    params.azi0 = np.random.uniform(0, 2 * pi, size=num_evts)
    params.pol0 = np.random.uniform(pi / 2, pi, size=num_evts)

    vert_ens = find_vertex_energy(params.z0, beam_enu0, beam_mass, beam_chg, gas)  # the total kinetic energies
    params.enu0 = find_proton_params(pi - params.pol0, beam_mass * p_mc2, proj_mass * p_mc2,
                                     proj_mass * p_mc2, beam_mass * p_mc2, vert_ens)[1] - proj_mass * p_mc2

    return params
