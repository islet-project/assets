#include <stdint.h>
#include <string.h>

#include <platform_def.h>

#include <common/debug.h>
#include <drivers/arm/mhu.h>
#include <drivers/arm/pl011.h>
#include <drivers/console.h>

#include "rss_comms_protocol.h"


static console_t data_channel;
static bool data_channel_initialized = false;

static void serial_lazy_initialize()
{
	if (data_channel_initialized)
		return;

	/* UART0 is BOOT and then passed to EFI/OS
	 * UART1 is RUN which is TF-A after passing 0 to EFI
	 * UART2 is TSP whatever that is, it's uninitialized
	 * UART3 is taken by TF-RMM
	 *
	 * Conclusion: use UART2
	 */
	int rc = console_pl011_register(V2M_IOFPGA_UART2_BASE,
	                                V2M_IOFPGA_UART2_CLK_IN_HZ,
	                                ARM_CONSOLE_BAUDRATE,
	                                &data_channel);
	if (rc != 1)
		panic();

	/* unregister the serial, we don't use it as console */
	if (console_is_registered(&data_channel)) {
		console_unregister(&data_channel);
	}
	data_channel.flags = 0;
	data_channel_initialized = true;
	NOTICE("[RSS_SERIAL] Serial initialized\n");
}

size_t mhu_get_max_message_size(void)
{
	return MAX(sizeof(struct serialized_rss_comms_msg_t),
	           sizeof(struct serialized_rss_comms_reply_t));
}

enum mhu_error_t mhu_send_data(const uint8_t *send_buffer, size_t size)
{
	size_t i;
	int ret;

	serial_lazy_initialize();

	for (i = 0; i < size; ++i) {
		ret = data_channel.putc(send_buffer[i], &data_channel);
		if (ret < 0) {
			NOTICE("[RSS_SERIAL] serial error: %d\n", ret);
			return MHU_ERR_GENERAL;
		}
	}

	NOTICE("[RSS_SERIAL] sent %lu bytes\n", size);

	return MHU_ERR_NONE;
}

enum mhu_error_t mhu_receive_data(uint8_t *receive_buffer, size_t *size)
{
	size_t read = 0;
	int c;

	serial_lazy_initialize();

	/* lock waiting for the first byte */
	do {
		c = data_channel.getc(&data_channel);
	} while (c < 0);
	receive_buffer[read++] = c;

	/* read the rest that's in the buffer */

	/* WARNING: Reading using the pl011 sometimes fails after some byte
	 * (aroung 5-6k) The first ifdeffed code below has no error checking
	 * but somehow works better. Reason unknown. Wrong UART control flow?
	 */

#if 0
	for (;;) {
		c = data_channel.getc(&data_channel);
		if (c < 0)
			break;
		receive_buffer[read++] = c;
	}
#else
	size_t buf_size = sizeof(struct serialized_rss_comms_reply_t);

	for (;;) {
		if (read >= buf_size) {
			NOTICE("[RSS_SERIAL] buffer overflow");
			return MHU_ERR_GENERAL;
		}
		int retry = 100;
		do {
			c = data_channel.getc(&data_channel);
			//NOTICE("[RSS_SERIAL] retry: %d, read char: %d\n", retry, c);
		} while (c < 0 && --retry > 0);
		if (retry == 0)
			break;
		receive_buffer[read++] = c;
	}
#endif

	*size = read;
	NOTICE("[RSS_SERIAL] read %lu bytes\n", read);

	return MHU_ERR_NONE;
}
