
/*
 * See comments in ganglia_interaction.c to understand the need for this file.
 */

#ifdef __cplusplus
extern "C" {
#endif

int
ganglia_reconfig(const char *config_file, Ganglia_pool *context, Ganglia_gmond_config *config, Ganglia_udp_send_channels *channels);

int
ganglia_send(Ganglia_pool context, Ganglia_udp_send_channels channels, const char *group, const char *name, const char *value, const char *type, int slope);

#ifdef __cplusplus
}
#endif

