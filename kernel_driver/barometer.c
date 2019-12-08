#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#define LF (10)
#define CMD_DELIMITER (":")
#define CMD_PRESSURE ("PRESS")

MODULE_AUTHOR("Piotr Topa <pt@approach.pl>");
MODULE_DESCRIPTION("Mock barometer driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1.0");
MODULE_ALIAS("sensor:barometer");

static ssize_t barometer_dev_fops_write(struct file*, const char __user*, size_t, loff_t*);
void barometer_dev_unregister(void);

static int barometer_dev_major = 0;
static dev_t barometer_dev_number;

static const char* barometer_dev_name = "TopaBarometer";
static const char* barometer_dev_classname = "barometer";

struct class* barometer_dev_class;
struct device* barometer_dev_device;

// /dev/barometer reading buffer
static char command_buffer[20];
static const size_t command_buffer_length = sizeof(command_buffer);
static int command_buffer_pos = 0;

// kernel interface, device file operations struct
static struct file_operations barometer_fops = {
  .owner  = THIS_MODULE,
  .write  = barometer_dev_fops_write
};

// commands
static void command_pressure(const char* value) {
  print(KERN_NOTICE "%s: parsing COMMAND_PRESSURE: %s", barometer_dev_name, value);
}

// command processor
static void command_buffer_parse_command(const char* key, const char* value) {
  printk(KERN_NOTICE "%s: parse command: %s -> %s", barometer_dev_name, key, value);

  if(strcmp(key, CMD_PRESSURE) == 0) {
    return command_pressure(value);
  }

  printk(KERN_DEBUG "%s: unknown command: %s", barometer_dev_name, key);
}

static void command_buffer_parse_line(const char* line) {
  char key[64];
  char value[64];
  char* delimiter;
  int len;

  delimiter = strstr(line, CMD_DELIMITER);
  if(delimiter == NULL) {
    printk(KERN_DEBUG "%s: invalid line format: %s", barometer_dev_name, line);
    return;
  }

  len = (int) (delimiter - line);
  memcpy(key, line, len);
  key[len] = 0;
  strcpy(value, (delimiter + 1));
  command_buffer_parse_command(key, value);
}

static void command_buffer_parse(void) {
  char line[command_buffer_length + 1];
  memcpy(line, command_buffer, command_buffer_pos);
  line[command_buffer_pos] = 0;
  command_buffer_pos = 0;
  command_buffer_parse_line(line);
}

// kernel device interface
static ssize_t barometer_dev_fops_write(struct file* fp, const char __user* user_buffer, size_t count, loff_t* position) {
  char buff[10];
  size_t i;
  printk(KERN_NOTICE "%s: fops_write, count: %i, pos: %i", barometer_dev_name, (int) count, (int) *position);
  count = min(count, sizeof(buff));
  count -= copy_from_user(buff, user_buffer, count);

  for(i = 0; i < count; i ++) {
    char data = buff[i];
    command_buffer[command_buffer_pos] = data;
    command_buffer_pos = (command_buffer_pos + 1) % command_buffer_length;
    if(data == LF) {
      command_buffer_parse();
    }
  }

  return count;
}

static int barometer_dev_register(void) {
  int result = 0;

  printk(KERN_NOTICE "%s: registering device...", barometer_dev_name);
  result = register_chrdev(barometer_dev_major, barometer_dev_name, &barometer_fops);
  if(result < 0) {
    printk(KERN_WARNING "%s: cannot register a device, with errorcode = %i", barometer_dev_name, result);
    return result;
  }
  barometer_dev_major = result;
  printk(KERN_NOTICE "%s: registered device, major = %i", barometer_dev_name, barometer_dev_major);
  barometer_dev_number = MKDEV(barometer_dev_major, 0);

  // Create /sys/class/barometer in preparateion for /dev/barometer
  barometer_dev_class = class_create(THIS_MODULE, barometer_dev_classname);
  if(IS_ERR(barometer_dev_class)) {
    printk(KERN_WARNING "%s: can't create a device class", barometer_dev_name);
    barometer_dev_unregister();
  }

  // Create /dev/barometer file for this device
  barometer_dev_device = device_create(barometer_dev_class, NULL, barometer_dev_number, NULL, barometer_dev_classname);
  if(IS_ERR(barometer_dev_device)) {
    printk(KERN_WARNING "%s: can't create device /dev/%s", barometer_dev_name, barometer_dev_classname);
    barometer_dev_unregister();
  }

  printk(KERN_NOTICE "%s: device /dev/%s created", barometer_dev_name, barometer_dev_classname);
  return 0;
}

void barometer_dev_unregister(void) {
  printk(KERN_NOTICE "%s: unregistering device...", barometer_dev_name);
  if(barometer_dev_device != NULL) {
    device_destroy(barometer_dev_class, barometer_dev_number);
    barometer_dev_device = NULL;
  }
  if(barometer_dev_class != NULL) {
    class_destroy(barometer_dev_class);
    barometer_dev_class = NULL;
  }
  if(barometer_dev_major != 0) {
    unregister_chrdev(barometer_dev_major, barometer_dev_name);
    barometer_dev_major = 0;
  }
  printk(KERN_NOTICE "%s: device unregistered", barometer_dev_name);
}


// kernel module interface
static int __init barometer_module_init(void) {
  printk(KERN_INFO "%s: initializing module...", barometer_dev_name);
  barometer_dev_register();
  printk(KERN_INFO "%s: module initialized", barometer_dev_name);
  return 0;
}

static void __exit barometer_module_exit(void) {
  printk(KERN_INFO "%s: wrapping module up...", barometer_dev_name);
  barometer_dev_unregister();
  printk(KERN_INFO "%s: module closed", barometer_dev_name);
}

// kernel module interface macros
module_init(barometer_module_init);
module_exit(barometer_module_exit)
