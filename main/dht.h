
void dht11_rmt_rx_init(int gpio_pin, int rmt_channel);
int dht11_rmt_rx(int gpio_pin, int rmt_channel, int *humidity, int *temp_x10);
