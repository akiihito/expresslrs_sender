#include "uart.hpp"

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <cstring>

// Linux-specific for custom baud rates
// Note: We use ioctl with TCGETS2/TCSETS2 directly to avoid header conflicts
#ifdef __linux__
#ifndef BOTHER
#define BOTHER 0010000
#endif
#ifndef TCGETS2
#define TCGETS2 _IOR('T', 0x2A, struct termios2)
#endif
#ifndef TCSETS2
#define TCSETS2 _IOW('T', 0x2B, struct termios2)
#endif

struct termios2 {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[19];
    speed_t c_ispeed;
    speed_t c_ospeed;
};
#endif

namespace elrs {
namespace uart {

UartDriver::UartDriver() : m_fd(-1), m_options{} {}

UartDriver::~UartDriver() {
    close();
}

UartDriver::UartDriver(UartDriver&& other) noexcept
    : m_fd(other.m_fd), m_device(std::move(other.m_device)), m_options(other.m_options) {
    other.m_fd = -1;
}

UartDriver& UartDriver::operator=(UartDriver&& other) noexcept {
    if (this != &other) {
        close();
        m_fd = other.m_fd;
        m_device = std::move(other.m_device);
        m_options = other.m_options;
        other.m_fd = -1;
    }
    return *this;
}

Result<void> UartDriver::open(const std::string& device, int baudrate) {
    UartOptions options;
    options.baudrate = baudrate;
    return open(device, options);
}

Result<void> UartDriver::open(const std::string& device, const UartOptions& options) {
    if (m_fd >= 0) {
        close();
    }

    m_fd = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_fd < 0) {
        return Result<void>::failure(
            ErrorCode::DeviceError,
            "Failed to open " + device + ": " + std::strerror(errno)
        );
    }

    m_device = device;
    m_options = options;

    auto result = configure(options.baudrate);
    if (!result.ok()) {
        close();
        return result;
    }

    return Result<void>::success();
}

void UartDriver::close() {
    if (m_fd >= 0) {
        flush();
        ::close(m_fd);
        m_fd = -1;
        m_device.clear();
    }
}

bool UartDriver::isOpen() const {
    return m_fd >= 0;
}

Result<void> UartDriver::configure(int baudrate) {
    struct termios tty{};

    if (tcgetattr(m_fd, &tty) != 0) {
        return Result<void>::failure(
            ErrorCode::DeviceError,
            "tcgetattr failed: " + std::string(std::strerror(errno))
        );
    }

    // 8N1 configuration
    tty.c_cflag &= ~PARENB;        // No parity
    tty.c_cflag &= ~CSTOPB;        // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;            // 8 data bits
    tty.c_cflag &= ~CRTSCTS;       // No hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable read, ignore modem controls

    // Raw input mode
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // Disable software flow control
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    // Raw output mode
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;

    // Non-blocking read
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    // Handle baud rate
    bool use_custom_baud = false;
    speed_t speed = B0;

    switch (baudrate) {
        case 9600:    speed = B9600; break;
        case 19200:   speed = B19200; break;
        case 38400:   speed = B38400; break;
        case 57600:   speed = B57600; break;
        case 115200:  speed = B115200; break;
        case 230400:  speed = B230400; break;
#ifdef B460800
        case 460800:  speed = B460800; break;
#endif
#ifdef B921600
        case 921600:  speed = B921600; break;
#endif
        case 420000:
            // 420000 is not a standard baud rate
            // Use custom baud rate via termios2 on Linux
            use_custom_baud = true;
            break;
        default:
            use_custom_baud = true;
            break;
    }

    if (!use_custom_baud) {
        cfsetispeed(&tty, speed);
        cfsetospeed(&tty, speed);

        if (tcsetattr(m_fd, TCSANOW, &tty) != 0) {
            return Result<void>::failure(
                ErrorCode::DeviceError,
                "tcsetattr failed: " + std::string(std::strerror(errno))
            );
        }
    } else {
#ifdef __linux__
        // Use termios2 for custom baud rates on Linux
        if (tcsetattr(m_fd, TCSANOW, &tty) != 0) {
            return Result<void>::failure(
                ErrorCode::DeviceError,
                "tcsetattr failed: " + std::string(std::strerror(errno))
            );
        }

        struct termios2 tty2{};
        if (ioctl(m_fd, TCGETS2, &tty2) < 0) {
            return Result<void>::failure(
                ErrorCode::DeviceError,
                "TCGETS2 failed: " + std::string(std::strerror(errno))
            );
        }

        tty2.c_cflag &= ~CBAUD;
        tty2.c_cflag |= BOTHER;
        tty2.c_ispeed = static_cast<speed_t>(baudrate);
        tty2.c_ospeed = static_cast<speed_t>(baudrate);

        if (ioctl(m_fd, TCSETS2, &tty2) < 0) {
            return Result<void>::failure(
                ErrorCode::DeviceError,
                "TCSETS2 failed (custom baud " + std::to_string(baudrate) + "): " +
                std::string(std::strerror(errno))
            );
        }
#else
        return Result<void>::failure(
            ErrorCode::DeviceError,
            "Custom baud rate " + std::to_string(baudrate) + " not supported on this platform"
        );
#endif
    }

    // Flush any existing data
    tcflush(m_fd, TCIOFLUSH);

    return Result<void>::success();
}

uint8_t UartDriver::invertByte(uint8_t byte) {
    // ビット順序を反転（LSB <-> MSB）
    byte = ((byte & 0xF0) >> 4) | ((byte & 0x0F) << 4);
    byte = ((byte & 0xCC) >> 2) | ((byte & 0x33) << 2);
    byte = ((byte & 0xAA) >> 1) | ((byte & 0x55) << 1);
    return ~byte;  // さらに論理反転
}

Result<size_t> UartDriver::write(const uint8_t* data, size_t len) {
    if (m_fd < 0) {
        return Result<size_t>::failure(ErrorCode::DeviceError, "Port not open");
    }

    ssize_t written;

    if (m_options.invert_tx) {
        // 信号反転が必要な場合、各バイトを反転
        std::vector<uint8_t> inverted(len);
        for (size_t i = 0; i < len; i++) {
            inverted[i] = invertByte(data[i]);
        }
        written = ::write(m_fd, inverted.data(), len);
    } else {
        written = ::write(m_fd, data, len);
    }

    if (written < 0) {
        return Result<size_t>::failure(
            ErrorCode::DeviceError,
            "Write failed: " + std::string(std::strerror(errno))
        );
    }

    return Result<size_t>::success(static_cast<size_t>(written));
}

Result<size_t> UartDriver::write(const std::vector<uint8_t>& data) {
    return write(data.data(), data.size());
}

Result<std::vector<uint8_t>> UartDriver::read(size_t max_len, int timeout_ms) {
    if (m_fd < 0) {
        return Result<std::vector<uint8_t>>::failure(
            ErrorCode::DeviceError, "Port not open"
        );
    }

    std::vector<uint8_t> buffer(max_len);

    // Use poll for timeout
    if (timeout_ms > 0) {
        struct pollfd pfd{};
        pfd.fd = m_fd;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, timeout_ms);
        if (ret < 0) {
            return Result<std::vector<uint8_t>>::failure(
                ErrorCode::DeviceError,
                "Poll failed: " + std::string(std::strerror(errno))
            );
        }
        if (ret == 0) {
            // Timeout - return empty buffer
            return Result<std::vector<uint8_t>>::success({});
        }
    }

    ssize_t bytes_read = ::read(m_fd, buffer.data(), max_len);
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return Result<std::vector<uint8_t>>::success({});
        }
        return Result<std::vector<uint8_t>>::failure(
            ErrorCode::DeviceError,
            "Read failed: " + std::string(std::strerror(errno))
        );
    }

    buffer.resize(static_cast<size_t>(bytes_read));

    // 信号反転が必要な場合
    if (m_options.invert_rx) {
        for (auto& byte : buffer) {
            byte = invertByte(byte);
        }
    }

    return Result<std::vector<uint8_t>>::success(std::move(buffer));
}

void UartDriver::flush() {
    if (m_fd >= 0) {
        tcdrain(m_fd);
        tcflush(m_fd, TCIOFLUSH);
    }
}

}  // namespace uart
}  // namespace elrs
