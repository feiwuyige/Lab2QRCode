#pragma once

#include <boost/mqtt5/mqtt_client.hpp>
#include <boost/mqtt5/types.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <vector>
#include <string>
#include <QMessageBox>

class MqttSubscriber {
public:

    using client_type = boost::mqtt5::mqtt_client<boost::asio::ip::tcp::socket>;

    using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;

    MqttSubscriber(const std::string& host, uint16_t port, const std::string& client_id, const MessageCallback& callback)
        : ioc_(std::make_unique<boost::asio::io_context>())
        , client_(*ioc_)
        , host_(host)
        , port_(port)
        , client_id_(client_id)
        , work_(boost::asio::make_work_guard(*ioc_))
        , callback_(callback)
    {}

    void subscribe(const std::string& topic) {
        topic_ = topic;


        client_.brokers(host_, port_)
            .credentials(client_id_)
            .async_run(boost::asio::detached);


        boost::mqtt5::subscribe_topic sub_topic(topic);

        // 订阅主题
        client_.async_subscribe(
            sub_topic,
            boost::mqtt5::subscribe_props{},
            [this](boost::mqtt5::error_code ec,
                std::vector<boost::mqtt5::reason_code> rcs,
                boost::mqtt5::suback_props props) {
                    if (!ec && !rcs.empty() && !rcs[0]) {
                        std::cout << "Subscribed successfully to topic: " << topic_ << std::endl;
                    }
                    else {
                        std::cout << "Failed to subscribe to topic: " << topic_ << ", error: " << ec.message() << std::endl;
                    }
            });

        std::cout << "Listening for messages on '" << topic << "'..." << std::endl;

        // 开始接收消息
        start_receive();

        // 在新线程中运行io_context
        runner_thread_ = std::thread([this]() {
            ioc_->run();
        });
    }

    void stop() {
        if (ioc_) {
            ioc_->stop();
        }

        if (runner_thread_.joinable()) {
            runner_thread_.join();
        }
    }

    ~MqttSubscriber() {
        stop();
    }

private:
    void start_receive() {
        client_.async_receive(
            [this](boost::mqtt5::error_code ec,
                std::string topic,
                std::string payload,
                boost::mqtt5::publish_props props) {
                    if (!ec) {
                        std::cout << topic << " " << payload << std::endl;
                        callback_(topic, payload);
                        // 继续接收下一条消息
                        start_receive();
                    }
                    else {
                        std::cout << "Error receiving message: " << ec.message() << std::endl;
                    }
            });
    }

    std::unique_ptr<boost::asio::io_context> ioc_;
    client_type client_;
    std::string host_;
    uint16_t port_;
    std::string client_id_;
    std::string topic_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_;
    std::thread runner_thread_;
    MessageCallback callback_;
};