#include "data_plane.h"
#include "bearer.h"
#include "pdn_connection.h"

// Конструктор data_plane: сохраняем ссылку на control_plane
data_plane::data_plane(control_plane &control_plane)
    : _control_plane(control_plane) {}

// Обработка uplink-трафика: от абонента в сторону APN
void data_plane::handle_uplink(uint32_t dp_teid, Packet &&packet) {
    auto bearer = _control_plane.find_bearer_by_dp_teid(dp_teid);
    if (!bearer) return; // Не нашли bearer — игнорируем пакет

    auto pdn = bearer->get_pdn_connection();
    if (!pdn) return; // У bearer нет связанной PDN-сессии

    if (!bearer->allow_uplink(packet.size())) return; // Применяем ограничение по скорости (rate limit)
    forward_packet_to_apn(pdn->get_apn_gw(), std::move(packet)); // Отправляем в сторону APN
}

// Обработка downlink-трафика: от APN в сторону абонента (UE)
void data_plane::handle_downlink(const boost::asio::ip::address_v4 &ue_ip, Packet &&packet) {
    auto pdn = _control_plane.find_pdn_by_ip_address(ue_ip);
    if (!pdn) return; // Не нашли PDN-сессию — дропаем пакет

    auto bearer = pdn->get_default_bearer();
    if (!bearer) return; // Нет default bearer — не знаем, как доставить

    if (!bearer->allow_downlink(packet.size())) return; // Rate limit для downlink
    forward_packet_to_sgw(pdn->get_sgw_address(), bearer->get_sgw_dp_teid(), std::move(packet));
}
