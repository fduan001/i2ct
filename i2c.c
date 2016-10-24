/* include header files */
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <linux/types.h> 
#include <sys/ioctl.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include <linux/i2c.h> 
#include <linux/i2c-dev.h> 

/* if some products need more i2c bus of FPGA or GPIO, define this macro as product need */
#ifndef I2C_MAX_BUS
#define I2C_MAX_BUS        90
#endif
#define I2C_DEV_NAME_MAX_SIZE (32)
#define IICDEVNAME "i2c"

#define I2C_ERR printf

#define I2C_OPS_READ          0
#define I2C_OPS_WRITE         1
#define I2C_SCAN_BUS          2
#define I2C_SCAN_CHIP         3

int i2c_write(int fd, unsigned char chipAddr, unsigned char alen, unsigned offset, 
		unsigned len, unsigned char *buffer, int verbose)
{
	int i = 0;
	int rc = 0;
	struct i2c_rdwr_ioctl_data msg_rdwr;
	struct i2c_msg             i2cmsg[2];

	unsigned char i2c_buffer[256]; /* buffer to send offset address & data */

	memset(i2cmsg, 0, sizeof(i2cmsg));
	memset(&msg_rdwr, 0, sizeof(msg_rdwr));

	switch (alen) {
		case 0:
			i2cmsg[0].addr = chipAddr;
			i2cmsg[0].flags = 0;
			i2cmsg[0].buf = buffer;
			i2cmsg[0].len = len;

			msg_rdwr.msgs = (struct i2c_msg*)i2cmsg;
			msg_rdwr.nmsgs = 1;
			break;
		case 1:
			i2c_buffer[i++] = (unsigned char)(offset & 0x00FF);
			i2cmsg[0].addr = chipAddr;

			memcpy(&i2c_buffer[i], buffer, len);

			i2cmsg[0].flags = 0; /* write flag = 0 */
			i2cmsg[0].len = len + i;
			i2cmsg[0].buf = i2c_buffer;

			msg_rdwr.msgs = (struct i2c_msg*)&i2cmsg;
			msg_rdwr.nmsgs = 1;
			break;
		case 2:
			i2cmsg[0].addr = (unsigned char)(offset >> 16);
			i2c_buffer[i++] = (unsigned char)(offset >> 8);
			i2c_buffer[i++] = (unsigned char)(offset);
			i2cmsg[0].addr |= chipAddr;
			memcpy(&i2c_buffer[i], buffer, len);

			i2cmsg[0].flags = 0; /* write flag = 0 */
			i2cmsg[0].len = len + i;
			i2cmsg[0].buf = i2c_buffer;

			msg_rdwr.msgs = (struct i2c_msg*)&i2cmsg;
			msg_rdwr.nmsgs = 1;
			break;
		default:
            if( verbose )
            {
                I2C_ERR("fail: offset length should be 0-3\n");
            }
			goto func_exit;
			break;
	}

	rc = ioctl(fd, I2C_RDWR, &msg_rdwr);
	if( rc < 0 )
	{
        if( verbose )
        {
            I2C_ERR("Faile, write to device 0x%x at address 0x%x, rc=%d\n", chipAddr, offset, rc);
        }
		goto func_exit;
	}

	return 0;
func_exit:
	return -1;
}

/******************************************************************************
 *  Name:  i2c_read
 *
 *  Description:  provide common interface to read from I2C slave ship by 
 *                specified bus
 *
 *  Inputs:     i2cBus - bus number of the I2C controller
 *              chipAddr - slave address for the accessing chip
 *              alen - the address length of the slave chip, 1 or 2
 *              offset - offset accessing from the chip
 *              len - length to be read
 *
 *  Outputs:    buffer - store the detail from the chip
 *  
 *  Returns:   0 - success others - failed 
 *
 ******************************************************************************/
int i2c_read(int fd, unsigned char chipAddr, unsigned char alen, unsigned offset, 
		unsigned len, unsigned char *buffer, int verbose)
{
	int rc = 0;
	int ii;

	unsigned char i2c_buffer[2];

	struct i2c_rdwr_ioctl_data msg_rdwr;
	struct i2c_msg             i2cmsg[2];

	switch(alen) {
		case 0:
			i2cmsg[0].addr = chipAddr;
			i2cmsg[0].flags = I2C_M_RD;
			i2cmsg[0].buf = buffer;
			i2cmsg[0].len = len;

			msg_rdwr.msgs = (struct i2c_msg*)i2cmsg;
			msg_rdwr.nmsgs = 1;
			break;

		case 1:
			i2c_buffer[0] = (unsigned char)(offset & 0x00FF);
			i2cmsg[0].addr = chipAddr;
			i2cmsg[0].flags = 0;
			i2cmsg[0].buf = i2c_buffer;
			i2cmsg[0].len = 1;

			i2cmsg[1].addr = i2cmsg[0].addr;
			i2cmsg[1].flags = I2C_M_RD; /* read flag */
			i2cmsg[1].buf = buffer;
			i2cmsg[1].len = len;

			msg_rdwr.msgs = (struct i2c_msg*)i2cmsg;
			msg_rdwr.nmsgs = 2;
			break;
		case 2:
			i2c_buffer[0] = (unsigned char)((offset & 0xFF00) >> 8);
			i2c_buffer[1] = (unsigned char)(offset & 0x00FF);
			i2cmsg[0].addr = (unsigned char)(offset >> 16); // block number
			i2cmsg[0].addr |= chipAddr;
			i2cmsg[0].flags = 0;
			i2cmsg[0].buf = i2c_buffer;
			i2cmsg[0].len = 2;

			i2cmsg[1].addr = i2cmsg[0].addr;
			i2cmsg[1].flags = I2C_M_RD; /* read flag */
			i2cmsg[1].buf = buffer;
			i2cmsg[1].len = len;

			msg_rdwr.msgs = (struct i2c_msg*)i2cmsg;
			msg_rdwr.nmsgs = 2;
			break;

		default:
			if( verbose) I2C_ERR("fail: offset length should be 0-3\n");
			goto func_exit;
			break;
	}

	for(ii=0; ii<10; ii++)
	{
		rc = ioctl(fd, I2C_RDWR, &msg_rdwr);
		if(rc >= 0)
		{	
			break;
		}	
		else
		{
            if( verbose )
            {
                I2C_ERR("%s: fails to read from I2C(%d) device 0x%x at address 0x%x in round %u\n", __func__, fd, chipAddr, offset, ii);
            }
		} 	 	
	}

	if( rc < 0 )
	{
        if( verbose )
        {
            I2C_ERR("Fail: fails to read from I2C(%d) device 0x%x at address 0x%x\n", fd, chipAddr, offset);
        }
		goto func_exit;
	}

	return 0;
func_exit:
	return -1;
}

void usage(void)
{
    printf("i2c utility for debug, Copyright: LGPLv2\n");
    printf("Author: fduan, dfxhappy@163.com\n");
    printf("Version: 0.0.1\n");
    printf("i2ct - r/w bus chip alen offset len data\n");
    printf("i2ct - p/s bus [chip] probe/scan the bus with/without the specified chip address\n");
	printf("PARAMETER\n");
	printf("\tops\n");
	printf("\t\tr - read from i2c device \n");
	printf("\t\tw - write to i2c device \n");
	printf("\tbus\n");
	printf("\t\tbus to access. \n");
	printf("\tchip\n");
	printf("\t\tchip address under the specified bus(i.e 0x48 and etc)\n");
	printf("\talen\n");
	printf("\t\taddress length, 0,1,2\n");
	printf("\toffset\n");
	printf("\t\toffset to be accessed of the chip internal\n");
	printf("\tlength\n");
	printf("\t\thow many bytes to be read\n");
	printf("\tbuffer\n");
	printf("\t\tbuffer to stored the data to be written to slave devices\n");
	printf("\ti.e:\n");
	printf("\t\ti2c w 1 0x50 0x3 10 0x26 0x57 0x78 - write 10 bytes\n");
	printf("\t\ti2c r 1 0x50 0x2 10 - read 10 bytes\n");
    printf("Alipay: 13851724905, Donation welcome If you think it's usefull\n");
    return ;
}

int main(int argc, char **argv)
{
    int i = 0;
    int bus = 0;
    int ops = 0;
    int fd = -1;
    unsigned int chip = 0;
    unsigned int alen = 1;
    unsigned int offset = 0;
    unsigned int length = 1; /* the least buffer size is 1 */
    unsigned int data = 0;
    unsigned char *buf = NULL;
    char fname[I2C_DEV_NAME_MAX_SIZE];
    char *option = NULL;
    int index = 0;
    int verbose = 1;

    if( argc <= 2 )
    {
        usage();
        return 255;
    }
    else
    {
        option = argv[1];
        if( strcmp(option, "r") == 0 )
        {
            ops = I2C_OPS_READ;
        }
        else if ( strcmp(option, "w") == 0 )
        {
            ops = I2C_OPS_WRITE;
        }
        else if ( (strcmp(option, "p") == 0) || (strcmp(option, "s") == 0) )
        {
            verbose = 0;
            ops = I2C_SCAN_BUS;
            if( argc == 4 )
            {
                ops = I2C_SCAN_CHIP; /* scan with specified chip address */
            }
        }
        else
        {
            usage();
            return 254;
        }

        option = argv[2];
        if(1!=sscanf(option,"0x%x", (unsigned int*)&bus) && 1!=sscanf(option, "%u", (unsigned int*)&bus))
        {
            usage();
            return 253;
        }

        if( ops != I2C_SCAN_BUS )
        {
            option = argv[3];
            if(1!=sscanf(option,"0x%x", (unsigned int*)&chip) && 1!=sscanf(option, "%u", (unsigned int*)&chip))
            {
                usage();
                return 252;
            }
        }

        if( (ops != I2C_SCAN_BUS) && (ops != I2C_SCAN_CHIP) )
        {
            option = argv[4];
            if(1!=sscanf(option,"0x%x", (unsigned int*)&alen) && 1!=sscanf(option, "%u", (unsigned int*)&alen))
            {
                usage();
                return 251;
            }

            option = argv[5];
            if(1!=sscanf(option,"0x%x", (unsigned int*)&offset) && 1!=sscanf(option, "%u", (unsigned int*)&offset))
            {
                usage();
                return 250;
            }

            option = argv[6];
            if(1!=sscanf(option,"0x%x", (unsigned int*)&length) && 1!=sscanf(option, "%u", (unsigned int*)&length))
            {
                length = 1;
            }
        }

        buf = (char*)malloc(length);
        if( buf == NULL )
        {
            I2C_ERR("failed to allocate buf\n");
            return 250;
        }
        memset(buf, 0, length);
        sprintf(fname, "/dev/i2c-%d", bus);
        fd = open(fname, O_RDWR);
        if( fd < 0 )
        {
            I2C_ERR("Critical Error: bus%d can't be opened, fname=%s\n", bus, fname);
            return 1;
        }

        if( ops == I2C_OPS_WRITE )
        {
            for(i = 0; i < length; ++i)
            {
                option = argv[7 + i];
                if( 7 + i > argc )
                {
                    break;
                }
                if(1!=sscanf(option,"0x%x", (unsigned int*)&data) && 1!=sscanf(option, "%u", (unsigned int*)&data))
                {
                    usage();
                    return 249;
                }
                else
                {
                    buf[i] = data;
                }
            }
#if 0
            for(i = 0; i < length; ++i)
            {
                printf("0x%02x ", buf[i]);
            }
#endif
            if( i2c_write(fd, chip, alen, offset, length, buf, verbose) != 0 )
            {
                I2C_ERR("Failed to write fd(%d), chip(%u), alen(%u), offset(%u), length(%u) \n",
                        fd, chip, alen, offset, length);
                return 2;
            }
            close(fd);
        }
        else if( ops == I2C_OPS_READ )
        {
            if( i2c_read(fd, chip, alen, offset, length, buf, verbose) != 0 )
            {
                I2C_ERR("Failed to read, fd(%d), chip(%u), alen(%u), offset(%u), length(%u) \n",
                        fd, chip, alen, offset, length);
                close(fd);
                return 3;
            }
            for(i = 0; i < length; ++i)
            {
                printf("0x%02x ", buf[i]);
            }
            printf("\n");
            close(fd);
        }
        else if( ops == I2C_SCAN_BUS )
        {
            for(i = 0; i < 127; ++i)
            {
                if( i2c_read(fd, i, 0, 0, 1, buf, verbose) == 0 )
                {
                    printf("chip(0x%02x) is on bus(%u)\n", i, bus);
                }
            }
        }
        else if( ops == 3 )
        {
            if( i2c_read(fd, chip, 0, 0, 1, buf, verbose) == 0 )
            {
                printf("chip(0x%02x) is on bus(%u)\n", chip, bus);
            }
        }
    }
    return 0;
}

