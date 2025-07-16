#include <bearer.h>
#include <pdn_connection.h>

// Конструктор теперь принимает std::shared_ptr вместо ссылки (pdn_connection&),
// что безопаснее и даёт возможность напрямую возвращать shared_ptr. (приводило к падению программы в тестах)
bearer::bearer(uint32_t dp_teid, std::shared_ptr<pdn_connection> pdn)
    : _dp_teid(dp_teid), _pdn(std::move(pdn)) {}

uint32_t bearer::get_sgw_dp_teid() const { return _sgw_dp_teid; }

void bearer::set_sgw_dp_teid(uint32_t sgw_cp_teid) { _sgw_dp_teid = sgw_cp_teid; }

uint32_t bearer::get_dp_teid() const { return _dp_teid; }

// Возвращает shared_ptr на связанный pdn_connection
// Без необходимости вызывать shared_from_this()
std::shared_ptr<pdn_connection> bearer::get_pdn_connection() const {
    return _pdn;
}

// Установка ограничения скорости uplink (в байтах/сек)
void bearer::set_uplink_rate(uint64_t bytes_per_sec) {
    _uplink_rate = bytes_per_sec;  // Сохраняем лимит пропускной способности
    _last_uplink_check = std::chrono::steady_clock::now(); // Запоминаем текущее время
    _uplink_tokens = bytes_per_sec; // Заполняем "токен-бакет" как будто прошла 1 секунда
}

// Установка ограничения скорости downlink (в байтах/сек)
void bearer::set_downlink_rate(uint64_t bytes_per_sec) {
    _downlink_rate = bytes_per_sec; // Запоминаем установленную скорость
    _last_downlink_check = std::chrono::steady_clock::now(); // Фиксируем текущее время как точку отсчёта
    _downlink_tokens = bytes_per_sec;  // Заполняем "бак" токенами — как если бы прошла 1 секунда
}

// Проверяет, можно ли сейчас отправить uplink-пакет указанного размера
// Алгоритм токен-бакета: если накоплено достаточно токенов — отправляем
bool bearer::allow_uplink(size_t packet_size) {
    if (_uplink_rate == 0) return true; // Если лимит не установлен, разрешаем всегда

    // Получаем текущее время
    auto now = std::chrono::steady_clock::now();
    // Вычисляем, сколько времени прошло с момента последней проверки
    double elapsed = std::chrono::duration<double>(now - _last_uplink_check).count();
    // Обновляем отметку времени
    _last_uplink_check = now;

    // Добавляем токены, накопившиеся за прошедшее время, но не больше максимума (1 секунда лимита)
    _uplink_tokens = std::min(_uplink_tokens + elapsed * _uplink_rate, static_cast<double>(_uplink_rate));
    // Если токенов достаточно для этого пакета — разрешаем и вычитаем
    if (_uplink_tokens >= packet_size) {
        _uplink_tokens -= packet_size;
        return true;
    }

    // Иначе запрещаем — лимит превышен
    return false;
}

// Аналогично — проверка и расход токенов для downlink
bool bearer::allow_downlink(size_t packet_size) {
    if (_downlink_rate == 0) return true; // Если лимит не задан — всегда разрешаем
    // Текущее время для расчёта интервала с последней проверки
    auto now = std::chrono::steady_clock::now();
    // Сколько времени прошло с последней проверки (в секундах)
    double elapsed = std::chrono::duration<double>(now - _last_downlink_check).count();
    // Обновляем точку отсчёта
    _last_downlink_check = now;

    // Добавляем токены за прошедшее время, но не больше, чем максимум (_downlink_rate)
    _downlink_tokens = std::min(_downlink_tokens + elapsed * _downlink_rate, static_cast<double>(_downlink_rate));
    // Если токенов достаточно — вычитаем размер пакета и разрешаем
    if (_downlink_tokens >= packet_size) {
        _downlink_tokens -= packet_size;
        return true;
    }
    // Иначе запрещаем — лимит превышен
    return false;
}