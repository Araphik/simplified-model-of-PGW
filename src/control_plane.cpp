#include "control_plane.h"
#include "bearer.h"

// Находит PDN-сессию по Control Plane TEID
std::shared_ptr<pdn_connection> control_plane::find_pdn_by_cp_teid(uint32_t cp_teid) const {
    auto it = _pdns.find(cp_teid);
    if (it != _pdns.end()) return it->second;
    return nullptr;
}

// Поиск PDN-сессии по IP-адресу UE
std::shared_ptr<pdn_connection> control_plane::find_pdn_by_ip_address(const boost::asio::ip::address_v4 &ip) const {
    auto it = _pdns_by_ue_ip_addr.find(ip);
    if (it != _pdns_by_ue_ip_addr.end()) return it->second;
    return nullptr;
}

// Поиск bearer по TEID на data plane
std::shared_ptr<bearer> control_plane::find_bearer_by_dp_teid(uint32_t dp_teid) const {
    auto it = _bearers.find(dp_teid);
    if (it != _bearers.end()) return it->second;
    return nullptr;
}

// Создание новой PDN-сессии
std::shared_ptr<pdn_connection> control_plane::create_pdn_connection(
    const std::string &apn,
    boost::asio::ip::address_v4 sgw_addr,
    uint32_t sgw_cp_teid
) {
    auto apn_it = _apns.find(apn);  // Проверяем, зарегистрирован ли APN
    if (apn_it == _apns.end()) return nullptr;

    const auto &apn_gw = apn_it->second;

    // Генерируем уникальный IP для UE на основе счётчика (например: 10.0.0.1, 10.0.0.2, ...)
    boost::asio::ip::address_v4 ue_ip_addr =
        boost::asio::ip::make_address_v4("10.0.0." + std::to_string(_next_ue_suffix++));

    // Создаём объект PDN и задаём ему адрес SGW
    auto pdn = pdn_connection::create(sgw_cp_teid, apn_gw, ue_ip_addr);
    pdn->set_sgw_addr(sgw_addr);

    // Сохраняем PDN-сессию в индексах
    _pdns[sgw_cp_teid] = pdn;
    _pdns_by_ue_ip_addr[ue_ip_addr] = pdn;

    return pdn;
}

// Удаляет PDN-сессию по TEID
void control_plane::delete_pdn_connection(uint32_t cp_teid) {
    auto pdn = find_pdn_by_cp_teid(cp_teid); // Находим PDN-сессию
    if (!pdn) return;

    if (auto bearer = pdn->get_default_bearer()) {
        _bearers.erase(bearer->get_dp_teid());  // Удаляем bearer из индекса, если он был
    }

    // Удаляем записи из всех словарей
    _pdns_by_ue_ip_addr.erase(pdn->get_ue_ip_addr());
    _pdns.erase(cp_teid);
}

// Создание bearer и его регистрация
std::shared_ptr<bearer> control_plane::create_bearer(
    const std::shared_ptr<pdn_connection> &pdn,
    uint32_t sgw_teid
) {
    auto b = std::make_shared<bearer>(sgw_teid, pdn); // Создаём bearer
    b->set_sgw_dp_teid(sgw_teid);  // Назначаем TEID от SGW
    _bearers[sgw_teid] = b; // Добавляем в глобальный словарь

    pdn->add_bearer(b); // Добавляем bearer в pdn_connection

    if (!pdn->get_default_bearer()) {
        pdn->set_default_bearer(b); // Если ещё нет default bearer — назначаем
    }

    return b;
}

// Удаление bearer по его TEID
void control_plane::delete_bearer(uint32_t dp_teid) {
    auto it = _bearers.find(dp_teid);
    if (it != _bearers.end()) {
        auto pdn = it->second->get_pdn_connection();
        if (pdn) {
            pdn->remove_bearer(dp_teid);  // Удаляем из pdn_connection
        }
        _bearers.erase(it);  // Удаляем из общего словаря
    }
}

// Регистрирует соответствие APN → IP-адрес шлюза
void control_plane::add_apn(std::string apn_name, boost::asio::ip::address_v4 apn_gateway) {
    _apns[std::move(apn_name)] = apn_gateway;
}
