#pragma once

#include <boost/asio/ip/address_v4.hpp>
#include <chrono>
#include <memory>

class pdn_connection;

class bearer {
public:
    // Конструктор теперь принимает shared_ptr вместо ссылки
    bearer(uint32_t dp_teid, std::shared_ptr<pdn_connection> pdn);

    [[nodiscard]] uint32_t get_sgw_dp_teid() const;
    void set_sgw_dp_teid(uint32_t sgw_cp_teid);

    [[nodiscard]] uint32_t get_dp_teid() const;

    [[nodiscard]] std::shared_ptr<pdn_connection> get_pdn_connection() const;

     // Установка лимита пропускной способности в байтах/сек (uplink/downlink)
    void set_uplink_rate(uint64_t bytes_per_sec);
    void set_downlink_rate(uint64_t bytes_per_sec);

     // Проверка, можно ли сейчас отправить пакет заданного размера (в байтах)
    bool allow_uplink(size_t packet_size);
    bool allow_downlink(size_t packet_size);

private:
    uint32_t _sgw_dp_teid{};
    uint32_t _dp_teid{};

    // Владение pdn_connection теперь задается через shared_ptr
    std::shared_ptr<pdn_connection> _pdn;

    // Максимальная скорость (байт/сек)
    uint64_t _uplink_rate = 0;
    uint64_t _downlink_rate = 0;

    // Текущие токены в баке (байты, с накоплением)
    double _uplink_tokens = 0;
    double _downlink_tokens = 0;

    // Время последней проверки (для восстановления токенов)
    std::chrono::steady_clock::time_point _last_uplink_check;
    std::chrono::steady_clock::time_point _last_downlink_check;
};
