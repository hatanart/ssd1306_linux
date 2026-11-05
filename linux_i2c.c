#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

static int file_i2c = -1;
static int use_smbus_block = 0;

static int smbus_i2c_block_write(uint8_t command, const uint8_t *data, uint8_t len)
{
	union i2c_smbus_data smbus_data;
	struct i2c_smbus_ioctl_data args;

	if (len == 0 || len > I2C_SMBUS_BLOCK_MAX)
		return -1;

	smbus_data.block[0] = len;
	memcpy(&smbus_data.block[1], data, len);
	args.read_write = I2C_SMBUS_WRITE;
	args.command = command;
	args.size = I2C_SMBUS_I2C_BLOCK_DATA;
	args.data = &smbus_data;
	return ioctl(file_i2c, I2C_SMBUS, &args);
}

uint8_t _i2c_init(int i2c, int dev_addr)
{
	if (file_i2c < 0)
	{
		char filename[32];
		unsigned long funcs = 0;
		sprintf(filename, "/dev/i2c-%d", i2c); // I2C bus number passed
		file_i2c = open(filename, O_RDWR);
		if (file_i2c < 0)
		{
			return 1;
		}
		if (ioctl(file_i2c, I2C_FUNCS, &funcs) < 0)
			goto fail;
		if (funcs & I2C_FUNC_I2C)
			use_smbus_block = 0;
		else if (funcs & I2C_FUNC_SMBUS_WRITE_I2C_BLOCK)
			use_smbus_block = 1;
		else
			goto fail;
		if (ioctl(file_i2c, I2C_SLAVE, dev_addr) < 0) // set slave address
			goto fail;
		return 0;
	fail:
		close(file_i2c);
		file_i2c = -1;
		return 1;
	}
	
	// assume done init already
	return 0;
}

uint8_t _i2c_close()
{
	if (file_i2c >= 0)
	{
		close(file_i2c);
		file_i2c = -1;
		return 0;
	}
	
	return 1;
}

uint8_t _i2c_write(uint8_t* ptr, int16_t len)
{
	if (file_i2c < 0 || ptr == 0 || len <= 1)
		return 1;

	if (!use_smbus_block)
		return write(file_i2c, ptr, len) == len ? 0 : 1;

	/* The first byte is the SSD1306 control byte.  SMBus sends it as
	 * its command byte; split the remaining payload into supported blocks. */
	int16_t offset = 1;
	while (offset < len)
	{
		uint8_t chunk = (uint8_t)(len - offset);
		if (chunk > I2C_SMBUS_BLOCK_MAX)
			chunk = I2C_SMBUS_BLOCK_MAX;
		if (smbus_i2c_block_write(ptr[0], &ptr[offset], chunk) < 0)
			return 1;
		offset += chunk;
	}
	return 0;
}

uint8_t _i2c_read(uint8_t *ptr, int16_t len)
{
	if (file_i2c < 0 || ptr == 0 || len <= 0 || use_smbus_block)
		return 1;
	return read(file_i2c, ptr, len) == len ? 0 : 1;
}
