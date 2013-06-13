
/*
 * See comments in ganglia_interaction.c to understand the need for this file.
 */

typedef struct Ganglia_pool_s* Ganglia_pool;
typedef struct Ganglia_gmond_config_s* Ganglia_gmond_config;
typedef struct Ganglia_udp_send_channels_s* Ganglia_udp_send_channels;

// From ganglia.h, which we can't include directly
enum ganglia_slope {
	GANGLIA_SLOPE_ZERO = 0,
	GANGLIA_SLOPE_POSITIVE,
	GANGLIA_SLOPE_NEGATIVE,
	GANGLIA_SLOPE_BOTH,
	GANGLIA_SLOPE_UNSPECIFIED,
	GANGLIA_SLOPE_DERIVATIVE
};


#ifdef __cplusplus
extern "C" {
#endif

int
ganglia_reconfig(const char *config_file, Ganglia_pool *context, Ganglia_gmond_config *config, Ganglia_udp_send_channels *channels);

int
ganglia_send(Ganglia_pool context, Ganglia_udp_send_channels channels, const char *group, const char *name, const char *value, const char *type, const char *units,int slope, const char *title, const char *desc, const char *spoof_host, int tmax, int dmax);

#ifdef __cplusplus
}
#endif

