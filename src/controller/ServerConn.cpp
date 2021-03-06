// This file is part of flobby (GPL v2 or later), see the LICENSE file

#include "ServerConn.h"
#include "IServerEvent.h"
#include "log/Log.h"

#include <boost/bind.hpp>
#include <thread>
#include <cassert>

using boost::asio::ip::tcp;


ServerConn::ServerConn(std::string const & host, std::string const & service, IServerEvent & iServerEvent):
    client_(iServerEvent),
    socket_(ioService_),
    resolver_(ioService_)
{
    tcp::resolver::query query(host, service);

    resolver_.async_resolve(
            query,
            boost::bind(&ServerConn::resolveHandler, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::iterator));

    // casting below is to avoid confusing eclipse when binding to overloaded run method
    auto f = boost::bind( static_cast<std::size_t (boost::asio::io_service::*)(void)>(&boost::asio::io_service::run), &ioService_);
    thread_.reset( new std::thread(f) );
}

ServerConn::~ServerConn()
{
    doClose();
    thread_->join();
    LOG(DEBUG) << "ServerConn destroyed";
}

void ServerConn::resolveHandler(const boost::system::error_code& error,
        boost::asio::ip::tcp::resolver::iterator iterator)
{
    if (!error)
    {
        boost::asio::async_connect(
                socket_,
                iterator,
                boost::bind(&ServerConn::connectHandler, this,
                        boost::asio::placeholders::error));
    }
    else
    {
        LOG(WARNING) << "resolve failed";
        client_.connected(false);
    }

}

void ServerConn::connectHandler(const boost::system::error_code& error)
{
    if (!error)
    {
        client_.connected(true);
        boost::asio::async_read_until(
                socket_,
                recvBuf_,
                '\n',
                boost::bind(&ServerConn::readHandler, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred) );
    }
    else
    {
        LOG(WARNING) << "connect failed";
        doClose();
    }
}

void ServerConn::readHandler(const boost::system::error_code& error, std::size_t bytes)
{
    if (!error)
    {
        std::istream is(&recvBuf_);
        std::string line;
        std::getline(is, line);
        client_.message(line);

        boost::asio::async_read_until(
                socket_,
                recvBuf_,
                '\n',
                boost::bind(&ServerConn::readHandler, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
    }
    else
    {
        LOG(DEBUG) << "read failed";
        doClose();
    }
}

void ServerConn::send(std::string msg)
{
    assert(msg.size() > 0);
    if (msg.back() != '\n')
    {
        msg.append(1, '\n');
    }
    LOG(DEBUG) << "ServerConn::send " << msg;
    ioService_.post(boost::bind(&ServerConn::doSend, this, msg));
}

void ServerConn::doSend(const std::string & msg)
{
    bool write_in_progress = !sendQueue_.empty();
    sendQueue_.push_back(msg);
    if (!write_in_progress)
    {
        boost::asio::async_write(
                socket_,
                boost::asio::buffer(sendQueue_.front().data(),
                        sendQueue_.front().length()),
                boost::bind(&ServerConn::writeHandler, this,
                        boost::asio::placeholders::error));
    }
}

void ServerConn::writeHandler(const boost::system::error_code& error)
{
    if (!error)
    {
        sendQueue_.pop_front();
        if (!sendQueue_.empty())
        {
            boost::asio::async_write(
                    socket_,
                    boost::asio::buffer(sendQueue_.front().data(),
                            sendQueue_.front().length()),
                    boost::bind(&ServerConn::writeHandler, this,
                            boost::asio::placeholders::error));
        }
    }
    else
    {
        doClose();
    }
}

void ServerConn::doClose()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (socket_.is_open())
    {
        socket_.close();
        LOG(DEBUG) << "closed";
        client_.connected(false);

    }
}
