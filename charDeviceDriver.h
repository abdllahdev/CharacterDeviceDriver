int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release};

static struct message
{
	char *data;
	size_t length;
	struct list_head list;
};

#define SUCCESS 0
#define DEVICE_NAME "opsysmem"		  // Dev name as it appears in /proc/devices
#define MESSAGE_SIZE 6 * 1024		  // Max size of the message from the device
#define MESSAGES_SIZE 4 * 1024 * 1024 // Max size of messages

static int major_number;
static int counter = 0;
static size_t messages_length = 0;
static int is_opened = 0;
