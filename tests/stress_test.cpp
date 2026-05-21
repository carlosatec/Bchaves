#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <csignal>
#include <chrono>

std::atomic<bool> g_stress_stop_requested{false};
std::atomic<int> g_signal_count{0};

void handle_stress_sig(int) {
    g_stress_stop_requested.store(true, std::memory_order_relaxed);
    g_signal_count.fetch_add(1, std::memory_order_relaxed);
}

void dummy_worker() {
    while (!g_stress_stop_requested.load(std::memory_order_relaxed)) {
        // Simular trabalho
        int dummy = 0;
        for (int i = 0; i < 1000; ++i) {
            dummy += i;
        }
    }
}

int main() {
    std::cout << "[*] Iniciando teste de estresse de sinais...\n";
    std::signal(SIGINT, handle_stress_sig);

    std::vector<std::thread> threads;
    for (int i = 0; i < 16; ++i) {
        threads.emplace_back(dummy_worker);
    }

    std::cout << "[*] Disparando sinais assincronos...\n";
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::raise(SIGINT);
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    std::cout << "[+] Teste concluido. Sinais recebidos: " << g_signal_count.load() << "\n";
    if (g_signal_count.load() > 0 && g_stress_stop_requested.load()) {
        std::cout << "[+] SUCESSO: Contexto de sinal resolvido sem deadlocks.\n";
        return 0;
    }
    return 1;
}
