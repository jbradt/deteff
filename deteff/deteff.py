from .trigger import TriggerSimulator
from pytpc.fitting.mixins import TrackerMixin, EventGeneratorMixin
from pytpc.relativity import find_proton_params
from pytpc.utilities import find_vertex_energy, read_lookup_table, find_exclusion_region
from pytpc.constants import pi, p_mc2
import numpy as np
import pandas as pd
import os
import logging

__all__ = ['TriggerSimulator', 'TriggerEfficiencySimulator', 'make_params']

logger = logging.getLogger(__name__)


class TriggerEfficiencySimulator(EventGeneratorMixin, TrackerMixin):
    def __init__(self, config, xcfg_path=None):
        super().__init__(config)

        self.padmap = read_lookup_table(config['padmap_path'])  # maps (cobo, asad, aget, ch) -> pad
        self.reverse_padmap = {v: k for k, v in self.padmap.items()}  # maps pad -> (cobo, asad, aget, ch)
        if xcfg_path is not None:
            self.excluded_pads, self.low_gain_pads = find_exclusion_region(xcfg_path, self.padmap)
            logger.info('Excluding pads from regions in file %s', os.path.basename(xcfg_path))
        else:
            self.excluded_pads = []
            self.low_gain_pads = []

        self.badpads = set(self.excluded_pads).union(set(self.low_gain_pads))
        logger.info('%d pads will be excluded', len(self.badpads))

        self.trigger = TriggerSimulator(config, self.reverse_padmap)

    def run_track(self, params):
        tr = self.tracker.track_particle(*params)
        evt = self.evtgen.make_event(tr[:, :3], tr[:, 4])

        for k in self.badpads.intersection(evt.keys()):
            del evt[k]

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
