#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <span>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <atomic>
#include <functional>
#include <iomanip>
#include <sstream>
#include <ranges>

// (a < b) <=> (a.price < b.price)
struct Product {
    std::string name;
    std::string description;
    int price;
    int quantity;

    Product() : price(0), quantity(0) {}
    Product(std::string n, std::string d, int p, int q)
        : name(std::move(n)), description(std::move(d)), price(p), quantity(q) {}

    [[nodiscard]] bool operator<(const Product& o) const { return price < o.price; }

    friend std::ostream& operator<<(std::ostream& os, const Product& p) {
        os << p.name << " | " << p.price << " руб. x" << p.quantity;
        return os;
    }
};

// atomic<int>, memory_order_relaxed
class Category {
    std::string name_;
    std::string description_;
    std::vector<Product> products_;

    static std::atomic<int> category_count_;
    static std::atomic<int> total_product_count_;

public:
    Category(std::string name, std::string desc,
             std::vector<Product> products = {})
        : name_(std::move(name)),
          description_(std::move(desc)),
          products_(std::move(products))
    {
        category_count_.fetch_add(1, std::memory_order_relaxed);
        total_product_count_.fetch_add(
            static_cast<int>(products_.size()), std::memory_order_relaxed);
    }

    ~Category() {
        category_count_.fetch_sub(1, std::memory_order_relaxed);
        total_product_count_.fetch_sub(
            static_cast<int>(products_.size()), std::memory_order_relaxed);
    }

    void add_product(Product&& p) {
        products_.push_back(std::move(p));
        total_product_count_.fetch_add(1, std::memory_order_relaxed);
    }

    void add_product(const Product& p) {
        products_.push_back(p);
        total_product_count_.fetch_add(1, std::memory_order_relaxed);
    }

    bool remove_product(std::string_view product_name) {
        auto it = std::ranges::find_if(products_,
            [product_name](const Product& p) { return p.name == product_name; });
        if (it == products_.end()) return false;
        products_.erase(it);
        total_product_count_.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }

    // V = sum_i (p_i * q_i)
    [[nodiscard]] long long total_value() const {
        return std::accumulate(products_.begin(), products_.end(), 0LL,
            [](long long acc, const Product& p) {
                return acc + static_cast<long long>(p.price) * p.quantity;
            });
    }

    // mu = (1/n) * sum_i p_i
    [[nodiscard]] double avg_price() const {
        if (products_.empty()) return 0.0;
        long long sum = std::accumulate(products_.begin(), products_.end(), 0LL,
            [](long long a, const Product& p) { return a + p.price; });
        return static_cast<double>(sum) / static_cast<double>(products_.size());
    }

    // sigma^2 = (1/n) * sum_i (p_i - mu)^2
    // M_1 = x_1
    // M_k = M_{k-1} + (x_k - M_{k-1})/k
    // S_k = S_{k-1} + (x_k - M_{k-1}) * (x_k - M_k)
    // sigma^2 = S_n / n
    [[nodiscard]] double price_variance() const {
        if (products_.size() < 2) return 0.0;
        double mean = 0.0, M2 = 0.0;
        int k = 0;
        for (const auto& p : products_) {
            ++k;
            double delta = p.price - mean;
            mean += delta / k;
            double delta2 = p.price - mean;
            M2 += delta * delta2;
        }
        return M2 / k;
    }

    // sort + lower_bound -> O(n log n + log n)
    [[nodiscard]] std::vector<Product> find_by_prefix(std::string_view prefix) const {
        auto sorted = products_;
        std::ranges::sort(sorted,
            [](const Product& a, const Product& b) { return a.name < b.name; });

        auto lo = std::lower_bound(sorted.begin(), sorted.end(), prefix,
            [](const Product& p, std::string_view val) {
                return p.name < val;
            });

        std::vector<Product> result;
        for (auto it = lo; it != sorted.end(); ++it) {
            if (std::string_view(it->name).substr(0, prefix.size()) == prefix)
                result.push_back(*it);
            else
                break;
        }
        return result;
    }

    // sigma(products, lo <= price <= hi), O(n)
    [[nodiscard]] std::vector<Product> get_in_range(int lo, int hi) const {
        std::vector<Product> result;
        std::ranges::copy_if(products_, std::back_inserter(result),
            [lo, hi](const Product& p) { return p.price >= lo && p.price <= hi; });
        return result;
    }

    void sort_by_price() {
        std::ranges::sort(products_);
    }

    // nth_element -> T(n) = O(n)
    [[nodiscard]] double median_price() const {
        if (products_.empty()) return 0.0;
        std::vector<int> prices;
        prices.reserve(products_.size());
        for (const auto& p : products_) prices.push_back(p.price);

        std::size_t n = prices.size();
        std::size_t mid = n / 2;
        std::nth_element(prices.begin(), prices.begin() + mid, prices.end());
        if (n % 2 == 1) return prices[mid];
        int hi = prices[mid];
        std::nth_element(prices.begin(), prices.begin() + mid - 1, prices.end());
        return (prices[mid - 1] + hi) / 2.0;
    }

    [[nodiscard]] int min_price() const {
        if (products_.empty()) return 0;
        return std::ranges::min_element(products_)->price;
    }

    [[nodiscard]] int max_price() const {
        if (products_.empty()) return 0;
        return std::ranges::max_element(products_)->price;
    }

    [[nodiscard]] std::string_view name() const { return name_; }
    [[nodiscard]] std::string_view description() const { return description_; }
    [[nodiscard]] std::span<const Product> products() const { return products_; }
    [[nodiscard]] std::size_t size() const { return products_.size(); }

    [[nodiscard]] static int category_count()       { return category_count_.load(); }
    [[nodiscard]] static int total_product_count()  { return total_product_count_.load(); }

    void print_all() const {
        std::cout << "=== " << name_ << " (" << description_ << ") ===\n";
        for (const auto& p : products_) std::cout << "  " << p << "\n";
    }

    void print_stats() const {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "--- Статистика: " << name_ << " ---\n";
        std::cout << "  |P|:            " << products_.size() << "\n";
        std::cout << "  V = sum(p*q):   " << total_value() << "\n";
        std::cout << "  min(price):     " << min_price() << "\n";
        std::cout << "  max(price):     " << max_price() << "\n";
        std::cout << "  mu:             " << avg_price() << "\n";
        std::cout << "  median:         " << median_price() << "\n";
        std::cout << "  sigma^2:        " << price_variance() << "\n";
        std::cout << "  sigma:          " << std::sqrt(price_variance()) << "\n";
    }
};

std::atomic<int> Category::category_count_{0};
std::atomic<int> Category::total_product_count_{0};

void run_menu(std::vector<Category>& cats) {
    while (true) {
        std::cout << "\n======== МЕНЮ ========\n"
                  << "1. Показать все категории\n"
                  << "2. Статистика по категории\n"
                  << "3. Добавить товар\n"
                  << "4. Удалить товар\n"
                  << "5. Найти по префиксу\n"
                  << "6. Фильтр по цене [lo, hi]\n"
                  << "7. Сортировка по цене\n"
                  << "8. Глобальная статистика\n"
                  << "0. Выход\n"
                  << ">>> ";
        int choice;
        if (!(std::cin >> choice)) break;

        auto pick_cat = [&]() -> Category* {
            std::cout << "Категории:\n";
            for (std::size_t i = 0; i < cats.size(); ++i)
                std::cout << "  " << i << ". " << cats[i].name() << "\n";
            std::cout << "Номер: ";
            int idx;
            std::cin >> idx;
            if (idx < 0 || idx >= static_cast<int>(cats.size())) {
                std::cout << "Неверный индекс.\n";
                return nullptr;
            }
            return &cats[idx];
        };

        if (choice == 0) break;

        if (choice == 1) {
            for (const auto& c : cats) c.print_all();
        }
        else if (choice == 2) {
            if (auto* c = pick_cat()) c->print_stats();
        }
        else if (choice == 3) {
            auto* c = pick_cat();
            if (!c) continue;
            std::cin.ignore();
            std::string nm, desc;
            int pr, qty;
            std::cout << "Название: ";   std::getline(std::cin, nm);
            std::cout << "Описание: ";   std::getline(std::cin, desc);
            std::cout << "Цена: ";       std::cin >> pr;
            std::cout << "Количество: "; std::cin >> qty;
            c->add_product(Product{std::move(nm), std::move(desc), pr, qty});
            std::cout << "Добавлено.\n";
        }
        else if (choice == 4) {
            auto* c = pick_cat();
            if (!c) continue;
            std::cin.ignore();
            std::string nm;
            std::cout << "Название товара для удаления: ";
            std::getline(std::cin, nm);
            std::cout << (c->remove_product(nm) ? "Удалено.\n" : "Не найдено.\n");
        }
        else if (choice == 5) {
            auto* c = pick_cat();
            if (!c) continue;
            std::cin.ignore();
            std::string pref;
            std::cout << "Префикс: ";
            std::getline(std::cin, pref);
            auto found = c->find_by_prefix(pref);
            if (found.empty()) {
                std::cout << "Ничего не найдено.\n";
            } else {
                for (const auto& p : found) std::cout << "  " << p << "\n";
            }
        }
        else if (choice == 6) {
            auto* c = pick_cat();
            if (!c) continue;
            int lo, hi;
            std::cout << "Мин. цена: "; std::cin >> lo;
            std::cout << "Макс. цена: "; std::cin >> hi;
            auto filtered = c->get_in_range(lo, hi);
            if (filtered.empty()) {
                std::cout << "Ничего в диапазоне [" << lo << ", " << hi << "].\n";
            } else {
                for (const auto& p : filtered) std::cout << "  " << p << "\n";
            }
        }
        else if (choice == 7) {
            if (auto* c = pick_cat()) {
                c->sort_by_price();
                c->print_all();
            }
        }
        else if (choice == 8) {
            std::cout << "|Categories|: " << Category::category_count() << "\n"
                      << "|Products|:   " << Category::total_product_count() << "\n";
            for (const auto& c : cats) c.print_stats();
        }
    }
}

int main() {
#ifdef _WIN32
    std::system("chcp 65001 > nul 2>&1");
#endif

    std::vector<Product> phones = {
        {"Samsung Galaxy S24",   "Флагман Samsung",      89990, 15},
        {"iPhone 15 Pro",        "Флагман Apple",       129990, 10},
        {"Xiaomi 14",            "Флагман Xiaomi",       54990,  25},
        {"Google Pixel 8",       "Чистый Android",       59990,  12},
        {"Samsung Galaxy A54",   "Среднебюджетный",      34990,  30},
    };

    std::vector<Product> tvs = {
        {"LG OLED C3 55\"",      "4K OLED",             109990,  5},
        {"Samsung QN85B 65\"",   "4K Neo QLED",         149990,  3},
        {"Xiaomi TV A Pro 55\"", "Бюджетный 4K",         39990, 20},
    };

    std::vector<Product> laptops = {
        {"MacBook Pro 14 M3",    "Apple Silicon",       199990,  7},
        {"ASUS ROG Strix G16",   "Игровой ноутбук",    139990,  8},
        {"Lenovo IdeaPad 3",     "Бюджетный ноутбук",   49990, 18},
        {"HP Pavilion 15",       "Универсальный",        64990, 14},
    };

    std::vector<Category> cats;
    cats.emplace_back("Смартфоны",  "Мобильные телефоны", std::move(phones));
    cats.emplace_back("Телевизоры", "Телевизоры и мониторы", std::move(tvs));
    cats.emplace_back("Ноутбуки",   "Портативные компьютеры", std::move(laptops));

    std::cout << "========== ДЕМОНСТРАЦИЯ ==========\n\n";

    for (const auto& c : cats) {
        c.print_all();
        std::cout << "\n";
    }

    std::cout << "\n";
    for (const auto& c : cats) {
        c.print_stats();
        std::cout << "\n";
    }

    std::cout << "|Categories|: " << Category::category_count() << "\n";
    std::cout << "|Products|:   " << Category::total_product_count() << "\n\n";

    std::cout << "--- sort by price ---\n";
    cats[0].sort_by_price();
    cats[0].print_all();
    std::cout << "\n";

    std::cout << "--- find_by_prefix('Samsung') ---\n";
    auto found = cats[0].find_by_prefix("Samsung");
    for (const auto& p : found) std::cout << "  " << p << "\n";
    std::cout << "\n";

    std::cout << "--- get_in_range(50000, 150000) ---\n";
    auto filtered = cats[2].get_in_range(50000, 150000);
    for (const auto& p : filtered) std::cout << "  " << p << "\n";
    std::cout << "\n";

    std::cout << "--- add_product(OnePlus 12) ---\n";
    cats[0].add_product(Product{"OnePlus 12", "Flagship killer", 59990, 10});
    std::cout << "|P_0|: " << cats[0].size() << "\n";
    std::cout << "|P|:   " << Category::total_product_count() << "\n\n";

    std::cout << "--- remove_product(OnePlus 12) ---\n";
    cats[0].remove_product("OnePlus 12");
    std::cout << "|P_0|: " << cats[0].size() << "\n";
    std::cout << "|P|:   " << Category::total_product_count() << "\n\n";

    std::cout << "========== ИНТЕРАКТИВНЫЙ РЕЖИМ ==========\n";
    run_menu(cats);

    return 0;
}
