import pytpc
from pytpc.constants import pi, p_mc2
import numpy as np
import pandas as pd


def find_proton_energy(th3, m1, m2, m3, m4, T):
    s = (m1 + m2)**2 + 2 * m2 * T
    pcm = np.sqrt(((s - m1**2 - m2**2)**2 - 4 * m1**2 * m2**2) / (4 * s))
    ppcm = np.sqrt(((s - m3**2 - m4**2)**2 - 4 * m3**2 * m4**2) / (4 * s))
    chi = np.log((pcm + np.sqrt(m2**2 + pcm**2)) / m2)
    E3cm = np.sqrt(ppcm**2 + m3**2)
#     print(np.sqrt(s), E3cm - m3)

    coshx = np.cosh(chi)
    sinhx = np.sinh(chi)

    root = np.sqrt(coshx**2 * (E3cm**2 + m3**2 * (-coshx**2 + np.cos(th3)**2 * sinhx**2)))
    denom = ppcm * (coshx**2 - np.cos(th3)**2 * sinhx**2)
    sinthcm = np.sin(th3) * (E3cm * np.cos(th3) * sinhx + root) / denom
    p3 = ppcm * sinthcm / np.sin(th3)
    E3 = np.sqrt(p3**2 + m3**2)
    return E3


def find_vertex_energy(beam_intercept, beam_enu0, beam_mass, beam_chg, gas):
    ei = beam_enu0 * beam_mass
    ri = gas.range(ei, beam_mass, beam_chg)  # this is in meters
    rf = ri - (1.0 - beam_intercept)
    ef = gas.inverse_range(rf, beam_mass, beam_chg)
    return ef


def make_params(beam_enu0, beam_mass, beam_chg, proj_mass, proj_chg, gas, num_evts):
    params = pd.DataFrame(0, columns=('x', 'y', 'z', 'enu', 'azi', 'pol'), index=range(num_evts))
    params.z = np.random.uniform(0, 1, size=num_evts)
    params.azi = np.random.uniform(0, 2*pi, size=num_evts)
    params.pol = np.random.uniform(pi/2, pi, size=num_evts)

    vert_ens = find_vertex_energy(params.z, beam_enu0, beam_mass, beam_chg, gas)  # the total kinetic energies
    params.enu = find_proton_energy(pi - params.pol, beam_mass*p_mc2, proj_mass*p_mc2,
                                    proj_mass*p_mc2, beam_mass*p_mc2, vert_ens) - proj_mass*p_mc2

    return params


def main():
    import yaml
    import argparse
    import sqlite3
    import h5py

    parser = argparse.ArgumentParser(description='A script to make parameters for deteff')
    parser.add_argument('config_path', help='Path to config file')
    parser.add_argument('eloss_path', help='Path to write energy loss info to')
    parser.add_argument('output_path', help='Path to output file')
    args = parser.parse_args()

    with open(args.config_path) as f:
        config = yaml.load(f)

    gas = pytpc.gases.InterpolatedGas(config['gas_name'], config['gas_pressure'])
    ens = np.arange(0, 100e3, dtype='int')
    eloss = gas.energy_loss(ens / 1000, config['mass_num'], config['charge_num'])
    with h5py.File(args.eloss_path, 'a') as h5file:
        if 'eloss' in h5file:
            del h5file['eloss']
        h5file.create_dataset('eloss', data=eloss)

    params = make_params(beam_enu0=config['beam_enu0'], beam_mass=config['beam_mass'], beam_chg=config['beam_charge'],
                         proj_mass=config['mass_num'], proj_chg=config['charge_num'], gas=gas,
                         num_evts=config['dist_num_pts'])

    with sqlite3.connect(args.output_path) as conn:
        params.to_sql('params', conn, index=False)


if __name__ == '__main__':
    main()
