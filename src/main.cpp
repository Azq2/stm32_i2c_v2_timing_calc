#include <cstdint>
#include <format>
#include <argparse/argparse.hpp>

#define I2C_STM32_VALID_TIMING_NBR                 128U

#define MSEC_PER_SEC	1000L
#define USEC_PER_MSEC	1000L
#define NSEC_PER_USEC	1000L
#define NSEC_PER_MSEC	1000000L
#define USEC_PER_SEC	1000000L
#define NSEC_PER_SEC	1000000000L
#define FSEC_PER_SEC	1000000000000000LL

#define I2C_STM32_SPEED_FREQ_STANDARD                0U    /* 100 kHz */
#define I2C_STM32_SPEED_FREQ_FAST                    1U    /* 400 kHz */
#define I2C_STM32_SPEED_FREQ_FAST_PLUS               2U    /* 1 MHz */
#define I2C_STM32_ANALOG_FILTER_DELAY_MIN            50U   /* ns */
#define I2C_STM32_ANALOG_FILTER_DELAY_MAX            260U  /* ns */
#define I2C_STM32_DIGITAL_FILTER_COEF                0U
#define I2C_STM32_PRESC_MAX                          16U
#define I2C_STM32_SCLDEL_MAX                         16U
#define I2C_STM32_SDADEL_MAX                         16U
#define I2C_STM32_SCLH_MAX                           256U
#define I2C_STM32_SCLL_MAX                           256U

static bool I2C_STM32_USE_ANALOG_FILTER = false;

/* I2C_DEVICE_Private_Types */
struct i2c_stm32_charac_t {
	uint32_t freq;       /* Frequency in Hz */
	uint32_t freq_min;   /* Minimum frequency in Hz */
	uint32_t freq_max;   /* Maximum frequency in Hz */
	uint32_t hddat_min;  /* Minimum data hold time in ns */
	uint32_t vddat_max;  /* Maximum data valid time in ns */
	uint32_t sudat_min;  /* Minimum data setup time in ns */
	uint32_t lscl_min;   /* Minimum low period of the SCL clock in ns */
	uint32_t hscl_min;   /* Minimum high period of SCL clock in ns */
	uint32_t trise;      /* Rise time in ns */
	uint32_t tfall;      /* Fall time in ns */
	uint32_t dnf;        /* Digital noise filter coefficient */
};

struct i2c_stm32_timings_t {
	uint32_t presc;      /* Timing prescaler */
	uint32_t tscldel;    /* SCL delay */
	uint32_t tsdadel;    /* SDA delay */
	uint32_t sclh;       /* SCL high period */
	uint32_t scll;       /* SCL low period */
};

/* I2C_DEVICE Private Constants */
static const struct i2c_stm32_charac_t i2c_stm32_charac[] = {
	{ // I2C_STM32_SPEED_FREQ_STANDARD
		.freq = 100000,
		.freq_min = 80000,
		.freq_max = 120000,
		.hddat_min = 0,
		.vddat_max = 3450,
		.sudat_min = 250,
		.lscl_min = 4700,
		.hscl_min = 4000,
		.trise = 640,
		.tfall = 20,
		.dnf = I2C_STM32_DIGITAL_FILTER_COEF,
	},
	{ // I2C_STM32_SPEED_FREQ_FAST
		.freq = 400000,
		.freq_min = 320000,
		.freq_max = 480000,
		.hddat_min = 0,
		.vddat_max = 900,
		.sudat_min = 100,
		.lscl_min = 1300,
		.hscl_min = 600,
		.trise = 250,
		.tfall = 100,
		.dnf = I2C_STM32_DIGITAL_FILTER_COEF,
	},
	{ // I2C_STM32_SPEED_FREQ_FAST_PLUS
		.freq = 1000000,
		.freq_min = 800000,
		.freq_max = 1200000,
		.hddat_min = 0,
		.vddat_max = 450,
		.sudat_min = 50,
		.lscl_min = 500,
		.hscl_min = 260,
		.trise = 60,
		.tfall = 100,
		.dnf = I2C_STM32_DIGITAL_FILTER_COEF,
	},
};

/*
 * Macro used to fix the compliance check warning :
 * "DEEP_INDENTATION: Too many leading tabs - consider code refactoring
 * in the i2c_compute_scll_sclh() function below
 */
#define I2C_LOOP_SCLH();						\
if ((tscl >= clk_min) &&					\
	(tscl <= clk_max) &&					\
	(tscl_h >= i2c_stm32_charac[i2c_speed].hscl_min) &&	\
	(ti2cclk < tscl_h)) {					\
		\
		int32_t error = (int32_t)tscl - (int32_t)ti2cspeed;	\
		\
		if (error < 0) {					\
			error = -error;					\
		}							\
		\
		if ((uint32_t)error < prev_error) {			\
			prev_error = (uint32_t)error;			\
			i2c_valid_timing[count].scll = scll;		\
			i2c_valid_timing[count].sclh = sclh;		\
			ret = count;					\
		}							\
	}

static struct i2c_stm32_timings_t i2c_valid_timing[I2C_STM32_VALID_TIMING_NBR];
static uint32_t i2c_valid_timing_nbr;
uint32_t i2c_compute_scll_sclh(uint32_t clock_src_freq, uint32_t i2c_speed)
{
	uint32_t ret = 0xFFFFFFFFU;
	uint32_t ti2cclk;
	uint32_t ti2cspeed;
	uint32_t prev_error;
	uint32_t dnf_delay;
	uint32_t clk_min, clk_max;
	uint32_t scll, sclh;
	uint32_t tafdel_min;

	ti2cclk = (NSEC_PER_SEC + (clock_src_freq / 2U)) / clock_src_freq;
	ti2cspeed = (NSEC_PER_SEC + (i2c_stm32_charac[i2c_speed].freq / 2U)) /
	i2c_stm32_charac[i2c_speed].freq;

	tafdel_min = (I2C_STM32_USE_ANALOG_FILTER == 1U) ?
	I2C_STM32_ANALOG_FILTER_DELAY_MIN :
	0U;

	/* tDNF = DNF x tI2CCLK */
	dnf_delay = i2c_stm32_charac[i2c_speed].dnf * ti2cclk;

	clk_max = NSEC_PER_SEC / i2c_stm32_charac[i2c_speed].freq_min;
	clk_min = NSEC_PER_SEC / i2c_stm32_charac[i2c_speed].freq_max;

	prev_error = ti2cspeed;

	for (uint32_t count = 0; count < I2C_STM32_VALID_TIMING_NBR; count++) {
		/* tPRESC = (PRESC+1) x tI2CCLK*/
		uint32_t tpresc = (i2c_valid_timing[count].presc + 1U) * ti2cclk;

		for (scll = 0; scll < I2C_STM32_SCLL_MAX; scll++) {
			/* tLOW(min) <= tAF(min) + tDNF + 2 x tI2CCLK + [(SCLL+1) x tPRESC ] */
			uint32_t tscl_l = tafdel_min + dnf_delay +
			(2U * ti2cclk) + ((scll + 1U) * tpresc);

			/*
			 * The I2CCLK period tI2CCLK must respect the following conditions:
			 * tI2CCLK < (tLOW - tfilters) / 4 and tI2CCLK < tHIGH
			 */
			if ((tscl_l > i2c_stm32_charac[i2c_speed].lscl_min) &&
				(ti2cclk < ((tscl_l - tafdel_min - dnf_delay) / 4U))) {
				for (sclh = 0; sclh < I2C_STM32_SCLH_MAX; sclh++) {
					/*
					 * tHIGH(min) <= tAF(min) + tDNF +
					 * 2 x tI2CCLK + [(SCLH+1) x tPRESC]
					 */
					uint32_t tscl_h = tafdel_min + dnf_delay +
					(2U * ti2cclk) + ((sclh + 1U) * tpresc);

					/* tSCL = tf + tLOW + tr + tHIGH */
					uint32_t tscl = tscl_l +
					tscl_h + i2c_stm32_charac[i2c_speed].trise +
					i2c_stm32_charac[i2c_speed].tfall;

					/* get timings with the lowest clock error */
					I2C_LOOP_SCLH();
				}
				}
		}
	}

	return ret;
}

/*
 * Macro used to fix the compliance check warning :
 * "DEEP_INDENTATION: Too many leading tabs - consider code refactoring
 * in the i2c_compute_presc_scldel_sdadel() function below
 */
#define I2C_LOOP_SDADEL();								\
											\
	if ((tsdadel >= (uint32_t)tsdadel_min) &&					\
		(tsdadel <= (uint32_t)tsdadel_max)) {					\
		if (presc != prev_presc) {						\
			i2c_valid_timing[i2c_valid_timing_nbr].presc = presc;		\
			i2c_valid_timing[i2c_valid_timing_nbr].tscldel = scldel;	\
			i2c_valid_timing[i2c_valid_timing_nbr].tsdadel = sdadel;	\
			prev_presc = presc;						\
			i2c_valid_timing_nbr++;						\
											\
			if (i2c_valid_timing_nbr >= I2C_STM32_VALID_TIMING_NBR) {	\
				break;							\
			}								\
		}									\
	}

/*
 * @brief  Compute PRESC, SCLDEL and SDADEL.
 * @param  clock_src_freq I2C source clock in Hz.
 * @param  i2c_speed I2C frequency (index).
 * @retval None.
 */
void i2c_compute_presc_scldel_sdadel(uint32_t clock_src_freq, uint32_t i2c_speed)
{
	uint32_t prev_presc = I2C_STM32_PRESC_MAX;
	uint32_t ti2cclk;
	int32_t  tsdadel_min, tsdadel_max;
	int32_t  tscldel_min;
	uint32_t presc, scldel, sdadel;
	uint32_t tafdel_min, tafdel_max;

	ti2cclk   = (NSEC_PER_SEC + (clock_src_freq / 2U)) / clock_src_freq;

	tafdel_min = (I2C_STM32_USE_ANALOG_FILTER == 1U) ?
		I2C_STM32_ANALOG_FILTER_DELAY_MIN : 0U;
	tafdel_max = (I2C_STM32_USE_ANALOG_FILTER == 1U) ?
		I2C_STM32_ANALOG_FILTER_DELAY_MAX : 0U;
	/*
	 * tDNF = DNF x tI2CCLK
	 * tPRESC = (PRESC+1) x tI2CCLK
	 * SDADEL >= {tf +tHD;DAT(min) - tAF(min) - tDNF - [3 x tI2CCLK]} / {tPRESC}
	 * SDADEL <= {tVD;DAT(max) - tr - tAF(max) - tDNF- [4 x tI2CCLK]} / {tPRESC}
	 */
	tsdadel_min = (int32_t)i2c_stm32_charac[i2c_speed].tfall +
		(int32_t)i2c_stm32_charac[i2c_speed].hddat_min -
		(int32_t)tafdel_min -
		(int32_t)(((int32_t)i2c_stm32_charac[i2c_speed].dnf + 3) *
		(int32_t)ti2cclk);

	tsdadel_max = (int32_t)i2c_stm32_charac[i2c_speed].vddat_max -
		(int32_t)i2c_stm32_charac[i2c_speed].trise -
		(int32_t)tafdel_max -
		(int32_t)(((int32_t)i2c_stm32_charac[i2c_speed].dnf + 4) *
		(int32_t)ti2cclk);

	/* {[tr+ tSU;DAT(min)] / [tPRESC]} - 1 <= SCLDEL */
	tscldel_min = (int32_t)i2c_stm32_charac[i2c_speed].trise +
		(int32_t)i2c_stm32_charac[i2c_speed].sudat_min;

	if (tsdadel_min <= 0) {
		tsdadel_min = 0;
	}

	if (tsdadel_max <= 0) {
		tsdadel_max = 0;
	}

	for (presc = 0; presc < I2C_STM32_PRESC_MAX; presc++) {
		for (scldel = 0; scldel < I2C_STM32_SCLDEL_MAX; scldel++) {
			/* TSCLDEL = (SCLDEL+1) * (PRESC+1) * TI2CCLK */
			uint32_t tscldel = (scldel + 1U) * (presc + 1U) * ti2cclk;

			if (tscldel >= (uint32_t)tscldel_min) {
				for (sdadel = 0; sdadel < I2C_STM32_SDADEL_MAX; sdadel++) {
					/* TSDADEL = SDADEL * (PRESC+1) * TI2CCLK */
					uint32_t tsdadel = (sdadel * (presc + 1U)) * ti2cclk;

					I2C_LOOP_SDADEL();
				}

				if (i2c_valid_timing_nbr >= I2C_STM32_VALID_TIMING_NBR) {
					return;
				}
			}
		}
	}
}

int main(int argc, char *argv[]) {
	argparse::ArgumentParser program("ptr89", "1.0.4");

	program.add_argument("-b", "--bus-clock")
		.default_value(8000000)
		.nargs(1)
		.scan<'i', int>();

	program.add_argument("-a", "--use-analog-filter")
		.default_value(false)
		.implicit_value(true)
		.nargs(0);

	program.add_argument("-s", "--speed")
		.default_value(100000)
		.nargs(1)
		.scan<'i', int>();

	program.add_argument("-h", "--help")
		.default_value(false)
		.implicit_value(true)
		.nargs(0);

	auto showHelp = []() {
		std::cerr << "Usage: stm32_i2c_v2_timing_calc [arguments]\n";
		std::cerr << "\n";
		std::cerr << "Global options:\n";
		std::cerr << "  -h, --help               show this help\n";
		std::cerr << "  -b, --bus-clock HZ       I2C bus clock [8000000]\n";
		std::cerr << "  -s, --speed HZ           I2C speed [100000]\n";
		std::cerr << "  -a, --use-analog-filter  use analog filter [false]\n";
		std::cerr << "\n";
	};

	try {
		program.parse_args(argc, argv);

		if (program.is_used("--help")) {
			showHelp();
			return 1;
		}

		I2C_STM32_USE_ANALOG_FILTER = program.get<bool>("--use-analog-filter");

		uint32_t clock = program.get<int>("--bus-clock");
		uint32_t i2c_freq = program.get<int>("--speed");

		std::cout << std::format("Use analog filter: {}", I2C_STM32_USE_ANALOG_FILTER) << "\n";
		std::cout << std::format("I2C bus clock: {} Hz", clock) << "\n";
		std::cout << std::format("I2C speed: {} Hz", i2c_freq) << "\n";
		std::cout << "------------------------------------" << "\n";

		for (int speed = 0 ; speed <= I2C_STM32_SPEED_FREQ_FAST_PLUS; speed++) {
			if ((i2c_freq >= i2c_stm32_charac[speed].freq_min) && (i2c_freq <= i2c_stm32_charac[speed].freq_max)) {
				i2c_compute_presc_scldel_sdadel(clock, speed);

				uint32_t idx = i2c_compute_scll_sclh(clock, speed);
				if (idx < I2C_STM32_VALID_TIMING_NBR) {
					uint32_t timing = ((i2c_valid_timing[idx].presc & 0x0FU) << 28) |
						((i2c_valid_timing[idx].tscldel & 0x0FU) << 20) |
						((i2c_valid_timing[idx].tsdadel & 0x0FU) << 16) |
						((i2c_valid_timing[idx].sclh & 0xFFU) << 8) |
						((i2c_valid_timing[idx].scll & 0xFFU) << 0);

					std::cout << std::format("I2C_TIMINGR: {:08X}", timing) << "\n";
					std::cout << std::format("Prescaler: {}", i2c_valid_timing[idx].presc) << "\n";
					std::cout << std::format("SCL low period: {}", i2c_valid_timing[idx].scll) << "\n";
					std::cout << std::format("SCL high period: {}", i2c_valid_timing[idx].sclh) << "\n";
					std::cout << std::format("SDA delay (data hold time): {}", i2c_valid_timing[idx].tsdadel) << "\n";
					std::cout << std::format("SCL delay (data setup time): {}", i2c_valid_timing[idx].tscldel) << "\n";
				}
				break;
			}
		}
	} catch (const std::exception &err) {
		std::cerr << "ERROR: " << err.what() << "\n\n";
		showHelp();
		return 1;
	}
}
