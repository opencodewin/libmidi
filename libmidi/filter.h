#ifndef ___FILTER_H_
#define ___FILTER_H_

/* Order of the FIR filter = 20 should be enough ! */
#define ORDER 20
#define ORDER2 ORDER / 2

void antialiasing(int16 *data, int32 data_length, int32 sample_rate, int32 output_rate);

#endif /* ___FILTER_H_ */
