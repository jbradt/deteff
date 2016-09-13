from atmc.mcopt_wrapper cimport EventGenerator, Tracker
cimport numpy as np
import numpy as np
from libc.math cimport round


cdef double e_charge = 1.602176565e-19


cdef _calculate_pad_threshold(int pad_thresh_LSB, int pad_thresh_MSB, double trig_discr_frac, double gain):
    cdef double pt = ((pad_thresh_MSB << 4) + pad_thresh_LSB)
    cdef double discrMax = trig_discr_frac * 4096  # in data ADC bins
    cdef double elecPerBin = gain / 4096 / e_charge
    return (pt / 128) * discrMax * elecPerBin


cdef class TriggerSimulator:

    cdef:
        EventGenerator evtgen
        Tracker tracker

        double write_clock
        double read_clock
        double master_clock

        double pad_threshold

        int trigger_width
        double trigger_height

        int multiplicity_threshold
        int multiplicity_window

        dict padmap

    def __cinit__(self, Tracker tracker, EventGenerator evtgen, dict config, dict padmap):
        self.tracker = tracker
        self.evtgen = evtgen

        self.write_clock = float(config['clock']) * 1e6
        self.read_clock = 25e6
        self.master_clock = 100e6

        cdef int pad_thresh_MSB = int(config['pad_thresh_MSB'])
        cdef int pad_thresh_LSB = int(config['pad_thresh_LSB'])
        cdef double trig_discr_frac = float(config['trigger_discriminator_fraction'])
        cdef double gain = float(config['electronics_gain'])
        self.pad_threshold = _calculate_pad_threshold(pad_thresh_LSB, pad_thresh_MSB, trig_discr_frac, gain)

        self.trigger_width = <int> round(float(config['trigger_signal_width']) * self.write_clock)
        self.trigger_height = 48

        self.multiplicity_threshold = config['multiplicity_threshold']
        self.multiplicity_window = <int> round(config['multiplicity_window'] / self.master_clock * self.write_clock)

        self.padmap = padmap

    cpdef np.ndarray[np.double_t, ndim=2] find_trigger_signals(self, dict evt):
        cdef np.ndarray[np.double_t, ndim=2] result = np.zeros((10, 512), dtype=np.double)

        cdef np.ndarray[np.double_t, ndim=1] trigSignal
        cdef int tbIdx, sqIdx
        cdef int padnum
        cdef np.ndarray[np.double_t, ndim=1] padsig
        cdef int cobo

        for padnum, padsig in evt.items():

            trigSignal = np.zeros(512, dtype=np.double)

            tbIdx = 0
            while tbIdx < 512:
                if padsig[tbIdx] > self.pad_threshold:
                    trigSignal[tbIdx:min(tbIdx + self.trigger_width, 512)] += self.trigger_height
                    tbIdx += self.trigger_width
                else:
                    tbIdx += 1

            if np.any(trigSignal > 0):
                cobo = self.padmap[padnum][0]
                result[cobo] += trigSignal

        return result

    cpdef np.ndarray[np.double_t, ndim=2] find_multiplicity_signals(self, np.ndarray[np.double_t, ndim=2] trig):
        cdef np.ndarray[np.double_t, ndim=2] result = np.zeros_like(trig, dtype=np.double)

        cdef int j, minIdx, maxIdx
        for j in range(512):
            minIdx = max(0, j - self.multiplicity_window)
            maxIdx = j
            result[:, j] = np.sum(trig[:, minIdx:maxIdx], axis=1) * self.read_clock / self.write_clock

        return result

    def did_trigger(self, np.ndarray[np.double_t, ndim=1] params):
        cdef np.ndarray[np.double_t, ndim=2] trmat = self.tracker.track_particle(params[0], params[1], params[2],
                                                                                 params[3], params[4], params[5])

        cdef dict evt = self.evtgen.make_event(trmat[:, :3], trmat[:, 4])

        cdef np.ndarray[np.double_t, ndim=2] trig, mult
        trig = self.find_trigger_signals(evt)
        mult = self.find_multiplicity_signals(trig)

        return np.any(np.max(mult, axis=1) > self.multiplicity_threshold)
