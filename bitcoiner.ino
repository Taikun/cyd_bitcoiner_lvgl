#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "wifi_credentials.h"

// Incluir las bibliotecas necesarias de LVGL
#include <lvgl.h>

// Definir el objeto de pantalla
static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf_1[LV_HOR_RES_MAX * 10];

// Definir los objetos de la interfaz de usuario
static lv_obj_t * scr;
static lv_obj_t * cryptoSelector;
static lv_obj_t * priceLabel;
static lv_obj_t * updateCountdownLabel;

// URLs para obtener los precios de las criptomonedas
const String bitcoinURL = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd";
const String ethereumURL = "https://api.coingecko.com/api/v3/simple/price?ids=ethereum&vs_currencies=usd";

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 300000; // 5 minutos en milisegundos

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado a la red WiFi.");
}

String getCryptoPrice(String url) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print("Error al deserializar la respuesta JSON: ");
        Serial.println(error.c_str());
        http.end();
        return "Error";
      }
      String cryptoId = url.indexOf("bitcoin") != -1 ? "bitcoin" : "ethereum";
      if (!doc.containsKey(cryptoId)) {
        Serial.println("La respuesta JSON no contiene la clave esperada");
        http.end();
        return "Error";
      }
      if (!doc[cryptoId].containsKey("usd")) {
        Serial.println("La respuesta JSON no contiene el precio en USD");
        http.end();
        return "Error";
      }
      float price = doc[cryptoId]["usd"];
      http.end();
      return String(price, 2);
    } else {
      Serial.print("Error en la solicitud HTTP: ");
      Serial.println(httpCode);
      http.end();
      return "Error";
    }
  } else {
    Serial.println("Error de conexión WiFi");
    return "No WiFi";
  }
}

static void updatePriceLabel(String crypto, String price) {
  lv_label_set_text_fmt(priceLabel, "%s: %s$", crypto.c_str(), price.c_str());
}

static void updateCountdown() {
  unsigned long elapsedTime = millis() - lastUpdate;
  unsigned long remainingTime = updateInterval - elapsedTime;
  int minutes = remainingTime / 60000;
  int seconds = (remainingTime % 60000) / 1000;

  lv_label_set_text_fmt(updateCountdownLabel, "Actualiza: %02d:%02d", minutes, seconds);
}

static void cryptoSelector_event_handler(lv_event_t * e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t * obj = lv_event_get_target(e);

  if (code == LV_EVENT_VALUE_CHANGED) {
    String selectedCrypto = lv_dropdown_get_selected_str(obj);
    String price = getCryptoPrice(selectedCrypto == "Bitcoin" ? bitcoinURL : ethereumURL);
    updatePriceLabel(selectedCrypto, price);
    lastUpdate = millis();
  }
}

void setup() {
  Serial.begin(115200);

  connectToWiFi();

  lv_init();

  // Inicializar la pantalla
  lv_disp_draw_buf_init(&disp_buf, buf_1, NULL, LV_HOR_RES_MAX * 10);

  // Configurar la pantalla
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.draw_buf = &disp_buf;
  disp_drv.flush_cb = &my_disp_flush;
  lv_disp_drv_register(&disp_drv);

  // Crear la pantalla principal
  scr = lv_obj_create(NULL, NULL);

  // Crear el selector de criptomonedas
  cryptoSelector = lv_dropdown_create(scr, NULL);
  lv_dropdown_set_options_static(cryptoSelector, "Bitcoin\nEthereum");
  lv_obj_set_event_cb(cryptoSelector, cryptoSelector_event_handler);
  lv_obj_align(cryptoSelector, NULL, LV_ALIGN_IN_TOP_MID, 0, 20);

  // Crear la etiqueta para mostrar el precio
  priceLabel = lv_label_create(scr, NULL);
  lv_label_set_text(priceLabel, "");
  lv_obj_align(priceLabel, NULL, LV_ALIGN_CENTER, 0, 0);

  // Crear la etiqueta para mostrar la cuenta regresiva de actualización
  updateCountdownLabel = lv_label_create(scr, NULL);
  lv_label_set_text(updateCountdownLabel, "");
  lv_obj_align(updateCountdownLabel, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -20);

  lv_scr_load(scr);
}

void loop() {
  lv_task_handler();

  if (millis() - lastUpdate >= updateInterval) {
    String selectedCrypto = lv_dropdown_get_selected_str(cryptoSelector);
    String price = getCryptoPrice(selectedCrypto == "Bitcoin" ? bitcoinURL : ethereumURL);
    updatePriceLabel(selectedCrypto, price);
    lastUpdate = millis();
  } else {
    updateCountdown();
  }

  delay(5);
}

void my_disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp_drv);
}
