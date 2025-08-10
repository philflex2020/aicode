Perfect. Here's a streamlined C++ tool using **libpcap** to **dissect `.pcap` files** and extract UIM2523 traffic, whether ASCII or binary.

---

## ✅ Goals:

* Open a `.pcap` file
* Look for TCP/UDP packets involving the UIM2523 device
* Print payloads (ASCII or hex) with timestamps and direction

---

## 🛠 Dependencies:

Install `libpcap`:

```bash
sudo apt install libpcap-dev
```

---

## 🧩 `pcap_uim2523_dissector.cpp`

```cpp
#include <pcap.h>
#include <iostream>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <cstring>

void print_payload(const u_char* data, int len) {
    bool is_ascii = true;
    for (int i = 0; i < len; ++i) {
        if (data[i] < 32 || data[i] > 126) {
            if (data[i] != '\r' && data[i] != '\n') {
                is_ascii = false;
                break;
            }
        }
    }

    if (is_ascii) {
        std::cout << "    ASCII: ";
        for (int i = 0; i < len; ++i)
            std::cout << (char)data[i];
        std::cout << "\n";
    } else {
        std::cout << "    HEX: ";
        for (int i = 0; i < len; ++i)
            printf("%02X ", data[i]);
        std::cout << "\n";
    }
}

void packet_handler(u_char* user, const struct pcap_pkthdr* h, const u_char* bytes) {
    const int ETHERNET_HEADER_SIZE = 14;

    if (h->caplen < ETHERNET_HEADER_SIZE + sizeof(struct ip)) return;

    const struct ip* iphdr = (struct ip*)(bytes + ETHERNET_HEADER_SIZE);
    if (iphdr->ip_p != IPPROTO_TCP) return;

    int ip_hdr_len = iphdr->ip_hl * 4;
    const struct tcphdr* tcph = (struct tcphdr*)(bytes + ETHERNET_HEADER_SIZE + ip_hdr_len);
    int tcp_hdr_len = tcph->th_off * 4;

    int payload_offset = ETHERNET_HEADER_SIZE + ip_hdr_len + tcp_hdr_len;
    int payload_len = h->caplen - payload_offset;

    if (payload_len <= 0) return;

    const u_char* payload = bytes + payload_offset;

    char src_ip[INET_ADDRSTRLEN], dst_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(iphdr->ip_src), src_ip, sizeof(src_ip));
    inet_ntop(AF_INET, &(iphdr->ip_dst), dst_ip, sizeof(dst_ip));

    printf("Time: %.6f  %s:%d -> %s:%d  Length: %d\n",
           h->ts.tv_sec + h->ts.tv_usec / 1000000.0,
           src_ip, ntohs(tcph->th_sport),
           dst_ip, ntohs(tcph->th_dport),
           payload_len);

    print_payload(payload, payload_len);
    std::cout << "--------------------------------------------\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " file.pcap\n";
        return 1;
    }

    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t* handle = pcap_open_offline(argv[1], errbuf);
    if (!handle) {
        std::cerr << "pcap_open_offline failed: " << errbuf << "\n";
        return 1;
    }

    pcap_loop(handle, 0, packet_handler, nullptr);
    pcap_close(handle);
    return 0;
}
```

---

## 📦 Build It:

```bash
g++ -o pcap_uim2523_dissector pcap_uim2523_dissector.cpp -lpcap
```

---

## 🧪 Run It:

```bash
./pcap_uim2523_dissector uim2523_trace.pcap
```

---

## 🧠 Example Output:

```
Time: 1717171234.123456  192.168.1.10:35231 -> 192.168.1.100:1001  Length: 18
    ASCII: AT+CANSEND=123#01
--------------------------------------------
Time: 1717171234.223456  192.168.1.100:1001 -> 192.168.1.10:35231  Length: 2
    ASCII: OK
--------------------------------------------
```

---

## 🔜 Possible Add-ons:

* Filter only certain IPs or ports
* Recognize and decode binary CAN format
* Export parsed CAN frames to CSV or JSON

Would you like to add **CAN ID/data decoding** if binary payloads are used, or extract/send the parsed data to a live visualizer or WebSocket?
