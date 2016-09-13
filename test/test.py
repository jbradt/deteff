

import deteff
import yaml
import numpy as np
import atmc



with open('/Users/josh/Documents/Code/ar40-aug15/fitters/config_e15503b.yml') as f:
    config = yaml.load(f)
with open('/Users/josh/Documents/Code/ar40-aug15/fitters/config_e15503b_macmini.yml') as f:
    patch = yaml.load(f)

config.update(patch)



ts = deteff.TriggerEfficiencySimulator(config)



ts.run_track(np.array([0, 0, 1, 1, 2, 3], dtype='float'))





