#pragma once

// Поднимает Wi-Fi station и блокируется до получения IP.
// Переподключается при обрывах сама (в обработчике событий).
void wifi_setup_connect(void);
