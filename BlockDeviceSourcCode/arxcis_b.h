#include <linux/delay.h>

//Configuration Register Offset
#define MOD_CONFIG_OFFSET   1
//Configuration Register Bit definitions
#define SAVDIS_BIT          0x40
#define INTMASK_BIT         0x80
#define AUTO_SAVE_BIT       0x01
#define CKE_BIT             0x08

/*Save Types*/
#define DISABLE_SAVE                0
#define HOST_INTERRUPT_SAVE         1
#define POWER_FAIL_SAVE             2
#define CKE_SAVE                    4
#define FORCE_SAVE_ONLY             8

#define	PROCESSOR_MAX	2

/* ArxCis device physical information */
struct arxcis_info_user {
    uint64_t start[PROCESSOR_MAX];
    uint64_t size[PROCESSOR_MAX];
    uint32_t num_nv_dimms;
    uint32_t driver_version;
    uint8_t  ddr;
};

/* ArxCis register transaction structure */
struct arxcis_reg {
    int dimm_index;
    unsigned char reg;
    unsigned char value;
};

#define ARXCISGETINFO        _IOR('M', 1, struct arxcis_info_user)
#define ARXCISGETREG         _IOWR('M', 2, struct arxcis_reg)
#define ARXCISPUTREG         _IOW('M', 3, struct arxcis_reg)
