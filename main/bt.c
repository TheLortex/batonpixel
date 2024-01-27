#include "bt.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "esp_bt.h"
// #include "esp_bt_main.h"
// #include "esp_gap_bt_api.h"
// #include "esp_bt_device.h"
// #include "esp_spp_api.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "nimble/ble.h"
#include "modlog/modlog.h"

#include "time.h"
#include "sys/time.h"
#include "led.h"

#define SPP_TAG "SPP"
#define SPP_SERVER_NAME "SPP_PIXELSTICK_SERVER"
#define EXAMPLE_DEVICE_NAME "🚥 Pixelstick 🚥"

/** Making sure client connects to server having L2CAP COC UUID */
#define L2CAP_COC_UUID 0x1812
uint16_t psm = 0x1002; // what is this number

static uint8_t own_addr_type;

void bleprph_advertise();

static QueueHandle_t led_event_queue;
static int conn_handle;

void print_addr(const void *addr)
{
    const uint8_t *u8p;

    u8p = addr;
    MODLOG_DFLT(INFO, "%02x:%02x:%02x:%02x:%02x:%02x",
                u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}

void bt_ack(unsigned int v)
{

    uint8_t response_ack[9];
    unsigned int length = 4;
    memcpy(response_ack, &length, sizeof(unsigned int));
    response_ack[4] = MSG_HEADER_PIXEL_ACK;
    unsigned int *ptr = (void *)&response_ack[5];
    *ptr = v;
    ESP_LOGI(SPP_TAG, "ACK %d", v);
    /*esp_spp_write(conn_handle, 9, response_ack);*/
}

void bt_recv(int bt_handle, int frame_len, unsigned char *frame)
{
    struct message led_event;

    if (frame[0] == MSG_HEADER_HELLO)
    {
        uint8_t response[9];
        unsigned int length = 4;
        memcpy(response, &length, sizeof(unsigned int));
        response[4] = MSG_HEADER_PIXEL_COUNT;
        unsigned int n_leds = LED_COUNT;
        memcpy(&response[5], &n_leds, sizeof(unsigned int));
        // esp_spp_write(bt_handle, 9, response);
    }
    else if (frame[0] == MSG_HEADER_PIXEL_BEGIN)
    {
        led_event.type = ANIMATE_BEGIN;
        led_event.animation_speed = frame[1];
        xQueueSend((QueueHandle_t)led_event_queue, &led_event, 100);
    }
    else if (frame[0] == MSG_HEADER_PIXEL_DATA)
    {
        led_event.type = ANIMATE;
        int len = frame_len - 1;
        led_event.http_animation.buffer = malloc(len);
        led_event.http_animation.width = len / (LED_COUNT * 3);

        memcpy(led_event.http_animation.buffer, (char *)&frame[1], len);

        xQueueSend((QueueHandle_t)led_event_queue, &led_event, 100);
    }
    else if (frame[0] == MSG_HEADER_PIXEL_END)
    {
        led_event.type = ANIMATE_END,
        led_event.animate_end_aborted = frame[1];
        xQueueSend((QueueHandle_t)led_event_queue, &led_event, 100);
    }
}

#define MAX_RECV_BUFFER 4000

static unsigned char receive_buffer[MAX_RECV_BUFFER];
static int receive_buffer_position = 0;

void bt_loop_frames(int bt_handle)
{
    int position = 0;

    while (position + 4 <= receive_buffer_position)
    {
        unsigned int frame_length = *((unsigned int *)&receive_buffer[position]);

        if (position + 4 + 1 + frame_length <= receive_buffer_position)
        {
            /* printf("FRAME: %d\n", frame_length); */
            bt_recv(bt_handle, 1 + frame_length, &receive_buffer[position + 4]);
            position = position + 4 + 1 + frame_length;
        }
        else
        {
            /* printf("WAITING: %d => %d (%d)\n", position, position + 4 + 1 + frame_length, receive_buffer_position); */
            break;
        }
    }

    if (position > 0)
    {
        memmove(receive_buffer, &receive_buffer[position], receive_buffer_position - position);
        receive_buffer_position = receive_buffer_position - position;
    }
}

void bt_handle(int bt_handle, int packet_len, unsigned char *packet)
{

    if (receive_buffer_position + packet_len > MAX_RECV_BUFFER)
    {
        ESP_LOGE(SPP_TAG, "receive buffer overrun %d", receive_buffer_position);
        return;
    }

    memcpy(&receive_buffer[receive_buffer_position], packet, packet_len);
    receive_buffer_position += packet_len;

    bt_loop_frames(bt_handle);
}

/*
static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    char bda_str[18] = {0};
    struct message led_event;

    switch (event)
    {
    case ESP_SPP_INIT_EVT:
        if (param->init.status == ESP_SPP_SUCCESS)
        {
            ESP_LOGI(SPP_TAG, "ESP_SPP_INIT_EVT");
            esp_spp_start_srv(sec_mask, role_slave, 0, SPP_SERVER_NAME);
        }
        else
        {
            ESP_LOGE(SPP_TAG, "ESP_SPP_INIT_EVT status:%d", param->init.status);
        }
        break;
    case ESP_SPP_DISCOVERY_COMP_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_DISCOVERY_COMP_EVT");
        break;
    case ESP_SPP_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_OPEN_EVT");
        break;
    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CLOSE_EVT status:%d handle:%d close_by_remote:%d", param->close.status,
                 (int)param->close.handle, param->close.async);

        led_event.type = WIFI_DISCONNECTED;
        xQueueSend((QueueHandle_t)led_event_queue, &led_event, 100);

        break;
    case ESP_SPP_START_EVT:
        if (param->start.status == ESP_SPP_SUCCESS)
        {
            ESP_LOGI(SPP_TAG, "ESP_SPP_START_EVT handle:%d sec_id:%d scn:%d", (int)param->start.handle, param->start.sec_id,
                     param->start.scn);
            esp_bt_dev_set_device_name(EXAMPLE_DEVICE_NAME);
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        }
        else
        {
            ESP_LOGE(SPP_TAG, "ESP_SPP_START_EVT status:%d", param->start.status);
        }
        break;
    case ESP_SPP_CL_INIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CL_INIT_EVT");
        break;
    case ESP_SPP_DATA_IND_EVT:
        *
         * We only show the data in which the data length is less than 128 here. If you want to print the data and
         * the data rate is high, it is strongly recommended to process them in other lower priority application task
         * rather than in this callback directly. Since the printing takes too much time, it may stuck the Bluetooth
         * stack and also have a effect on the throughput!
         *
        ESP_LOGI(SPP_TAG, "ESP_SPP_DATA_IND_EVT len:%d handle:%d",
                 param->data_ind.len, (int)param->data_ind.handle);
        if (param->data_ind.len < 128)
        {
            esp_log_buffer_hex("", param->data_ind.data, param->data_ind.len);
        }

        bt_handle(param->data_ind.handle, param->data_ind.len, param->data_ind.data);
        break;
    case ESP_SPP_CONG_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CONG_EVT");
        break;
    case ESP_SPP_WRITE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_WRITE_EVT");
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_OPEN_EVT status:%d handle:%d, rem_bda:[%s]", (int)param->srv_open.status,
                 (int)param->srv_open.handle, bda2str(param->srv_open.rem_bda, bda_str, sizeof(bda_str)));

        conn_handle = param->srv_open.handle;

        led_event.type = WIFI_CONNECTED;
        xQueueSend((QueueHandle_t)led_event_queue, &led_event, 100);

        break;
    case ESP_SPP_SRV_STOP_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_STOP_EVT");
        break;
    case ESP_SPP_UNINIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_UNINIT_EVT");
        break;
    default:
        break;
    }
}*/

/*
void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    char bda_str[18] = {0};

    switch (event)
    {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
    {
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGI(SPP_TAG, "authentication success: %s bda:[%s]", param->auth_cmpl.device_name,
                     bda2str(param->auth_cmpl.bda, bda_str, sizeof(bda_str)));
        }
        else
        {
            ESP_LOGE(SPP_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
        }
        break;
    }
    case ESP_BT_GAP_PIN_REQ_EVT:
    {
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
        if (param->pin_req.min_16_digit)
        {
            ESP_LOGI(SPP_TAG, "Input pin code: 0000 0000 0000 0000");
            esp_bt_pin_code_t pin_code = {0};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
        }
        else
        {
            ESP_LOGI(SPP_TAG, "Input pin code: 1234");
            esp_bt_pin_code_t pin_code;
            pin_code[0] = '1';
            pin_code[1] = '2';
            pin_code[2] = '3';
            pin_code[3] = '4';
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
        }
        break;
    }

#if (CONFIG_BT_SSP_ENABLED == true)
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %d", (int)param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%d", (int)param->key_notif.passkey);
        break;
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
        break;
#endif

    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode:%d bda:[%s]", param->mode_chg.mode,
                 bda2str(param->mode_chg.bda, bda_str, sizeof(bda_str)));
        break;

    default:
    {
        ESP_LOGI(SPP_TAG, "event: %d", event);
        break;
    }
    }
    return;
}*/

#define COC_BUF_COUNT (20 * MYNEWT_VAL(BLE_L2CAP_COC_MAX_NUM))
#define MTU 512

static os_membuf_t sdu_coc_mem[OS_MEMPOOL_SIZE(COC_BUF_COUNT, MTU)];
static struct os_mempool sdu_coc_mbuf_mempool;
static struct os_mbuf_pool sdu_os_mbuf_pool;
static uint16_t peer_sdu_size;

void ble_store_config_init(void);

/**
 * Logs information about a connection to the console.
 */
static void
bleprph_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    MODLOG_DFLT(INFO, "handle=%d our_ota_addr_type=%d our_ota_addr=",
                desc->conn_handle, desc->our_ota_addr.type);
    print_addr(desc->our_ota_addr.val);
    MODLOG_DFLT(INFO, " our_id_addr_type=%d our_id_addr=",
                desc->our_id_addr.type);
    print_addr(desc->our_id_addr.val);
    MODLOG_DFLT(INFO, " peer_ota_addr_type=%d peer_ota_addr=",
                desc->peer_ota_addr.type);
    print_addr(desc->peer_ota_addr.val);
    MODLOG_DFLT(INFO, " peer_id_addr_type=%d peer_id_addr=",
                desc->peer_id_addr.type);
    print_addr(desc->peer_id_addr.val);
    MODLOG_DFLT(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                      "encrypted=%d authenticated=%d bonded=%d\n",
                desc->conn_itvl, desc->conn_latency,
                desc->supervision_timeout,
                desc->sec_state.encrypted,
                desc->sec_state.authenticated,
                desc->sec_state.bonded);
}

static int
bleprph_l2cap_coc_accept(uint16_t conn_handle, uint16_t peer_mtu,
                         struct ble_l2cap_chan *chan)
{
    struct os_mbuf *sdu_rx;

    console_printf("LE CoC accepting, chan: 0x%08lx, peer_mtu %d\n",
                   (uint32_t)chan, peer_mtu);

    sdu_rx = os_mbuf_get_pkthdr(&sdu_os_mbuf_pool, 0);
    if (!sdu_rx)
    {
        return BLE_HS_ENOMEM;
    }

    return ble_l2cap_recv_ready(chan, sdu_rx);
}

/**
 * The nimble host executes this callback when a L2CAP  event occurs.  The
 * application associates a L2CAP event callback with each connection that is
 * established.  blecent_l2cap_coc uses the same callback for all connections.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                                  blecent_l2cap_coc.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular L2CAP event being signalled.
 */
static int
bleprph_l2cap_coc_event_cb(struct ble_l2cap_event *event, void *arg)
{
    struct ble_l2cap_chan_info chan_info;

    switch (event->type)
    {
    case BLE_L2CAP_EVENT_COC_CONNECTED:
        if (event->connect.status)
        {
            console_printf("LE COC error: %d\n", event->connect.status);
            return 0;
        }

        if (ble_l2cap_get_chan_info(event->connect.chan, &chan_info))
        {
            assert(0);
        }

        console_printf("LE COC connected, conn: %d, chan: %p, psm: 0x%02x,"
                       " scid: 0x%04x, "
                       "dcid: 0x%04x, our_mps: %d, our_mtu: %d,"
                       " peer_mps: %d, peer_mtu: %d\n",
                       event->connect.conn_handle, event->connect.chan,
                       chan_info.psm, chan_info.scid, chan_info.dcid,
                       chan_info.our_l2cap_mtu, chan_info.our_coc_mtu,
                       chan_info.peer_l2cap_mtu, chan_info.peer_coc_mtu);

        return 0;

    case BLE_L2CAP_EVENT_COC_DISCONNECTED:
        console_printf("LE CoC disconnected, chan: %p\n",
                       event->disconnect.chan);

        return 0;

    case BLE_L2CAP_EVENT_COC_DATA_RECEIVED:
        if (event->receive.sdu_rx != NULL)
        {
            MODLOG_DFLT(INFO, "Data received : ");
            for (int i = 0; i < event->receive.sdu_rx->om_len; i++)
            {
                console_printf("%d ", event->receive.sdu_rx->om_data[i]);
            }
            os_mbuf_free(event->receive.sdu_rx);
        }
        fflush(stdout);
        bleprph_l2cap_coc_accept(event->receive.conn_handle,
                                 peer_sdu_size,
                                 event->receive.chan);
        return 0;

    case BLE_L2CAP_EVENT_COC_ACCEPT:
        peer_sdu_size = event->accept.peer_sdu_size;
        bleprph_l2cap_coc_accept(event->accept.conn_handle,
                                 event->accept.peer_sdu_size,
                                 event->accept.chan);
        return 0;

    default:
        return 0;
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * bleprph uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unused by
 *                                  bleprph.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
bleprph_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        MODLOG_DFLT(INFO, "connection %s; status=%d ",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);
        if (event->connect.status == 0)
        {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            bleprph_print_conn_desc(&desc);
        }
        MODLOG_DFLT(INFO, "\n");

        if (event->connect.status != 0)
        {
            /* Connection failed; resume advertising. */
            bleprph_advertise();
        }
        else
        {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            bleprph_print_conn_desc(&desc);
            rc = ble_l2cap_create_server(psm, MTU, bleprph_l2cap_coc_event_cb, NULL);
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        bleprph_print_conn_desc(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

        /* Connection terminated; resume advertising. */
        bleprph_advertise();

        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        MODLOG_DFLT(INFO, "connection updated; status=%d ",
                    event->conn_update.status);
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        assert(rc == 0);
        bleprph_print_conn_desc(&desc);
        MODLOG_DFLT(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        MODLOG_DFLT(INFO, "advertise complete; reason=%d",
                    event->adv_complete.reason);
#if !CONFIG_EXAMPLE_EXTENDED_ADV
        bleprph_advertise();
#endif
        return 0;

    default:
        return 0;
    }

    return 0;
}

void bleprph_host_task(void *param)
{
    ESP_LOGI(SPP_TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

static void
bleprph_l2cap_coc_mem_init(void)
{
    int rc;
    rc = os_mempool_init(&sdu_coc_mbuf_mempool, COC_BUF_COUNT, MTU, sdu_coc_mem,
                         "coc_sdu_pool");
    assert(rc == 0);
    rc = os_mbuf_pool_init(&sdu_os_mbuf_pool, &sdu_coc_mbuf_mempool, MTU,
                           COC_BUF_COUNT);
    assert(rc == 0);
}

static void
bleprph_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

// TODO: checkout extended advertisement
/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
void bleprph_advertise()
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;
    int rc;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    fields.uuids16 = (ble_uuid16_t[]){
        BLE_UUID16_INIT(L2CAP_COC_UUID)};
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, bleprph_gap_event, NULL);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

static void
bleprph_on_sync(void)
{
    int rc;

    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Printing ADDR */
    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

    MODLOG_DFLT(INFO, "Device Address: ");
    print_addr(addr_val);
    MODLOG_DFLT(INFO, "\n");
    /* Begin advertising. */
#if CONFIG_EXAMPLE_EXTENDED_ADV
    ext_bleprph_advertise();
#else
    bleprph_advertise();
#endif
}

void bt_init(QueueHandle_t _led_event_queue)
{

    led_event_queue = _led_event_queue;

    char bda_str[18] = {0};

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize NimBLE
    ret = nimble_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "NimBLE port init failed");
        return;
    }

    // Initialize the NimBLE host configuration
    // ble_hs_cfg.reset_cb = bleprph_on_reset; // callback
    ble_hs_cfg.sync_cb = bleprph_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;

    bleprph_l2cap_coc_mem_init();

    ret = ble_svc_gap_device_name_set(EXAMPLE_DEVICE_NAME);
    if (ret != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "Failed to set device name, error code = %x", ret);
        return;
    }

    ble_store_config_init();
    nimble_port_freertos_init(bleprph_host_task);

    /*
        if ((ret = esp_bluedroid_init()) != ESP_OK)
        {
            ESP_LOGE(SPP_TAG, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
            return;
        }

        if ((ret = esp_bluedroid_enable()) != ESP_OK)
        {
            ESP_LOGE(SPP_TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
            return;
        }

        if ((ret = esp_bt_gap_register_callback(esp_bt_gap_cb)) != ESP_OK)
        {
            ESP_LOGE(SPP_TAG, "%s gap register failed: %s\n", __func__, esp_err_to_name(ret));
            return;
        }

        if ((ret = esp_spp_register_callback(esp_spp_cb)) != ESP_OK)
        {
            ESP_LOGE(SPP_TAG, "%s spp register failed: %s\n", __func__, esp_err_to_name(ret));
            return;
        }

        if ((ret = esp_spp_init(esp_spp_mode)) != ESP_OK)
        {
            ESP_LOGE(SPP_TAG, "%s spp init failed: %s\n", __func__, esp_err_to_name(ret));
            return;
        }

        * Set default parameters for Secure Simple Pairing
        esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
        esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
        esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

        *
         * Set default parameters for Legacy Pairing
         * Use variable pin, input pin code when pairing
         *
        esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
        esp_bt_pin_code_t pin_code;
        esp_bt_gap_set_pin(pin_type, 0, pin_code);

        ESP_LOGI(SPP_TAG, "Own address:[%s]", bda2str((uint8_t *)esp_bt_dev_get_address(), bda_str, sizeof(bda_str)));
        */
}