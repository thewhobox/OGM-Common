#include "OpenKNX/Log/Logger.h"
#include "OpenKNX/Facade.h"

#ifdef OPENKNX_RTT
#include "SEGGER_RTT.h"
#endif

#if defined(OPENKNX_TRACE1) || defined(OPENKNX_TRACE2) || defined(OPENKNX_TRACE3) || defined(OPENKNX_TRACE4) || defined(OPENKNX_TRACE5)
#include <Regexp.h>
#endif

namespace OpenKNX
{
    namespace Log
    {
        Logger::Logger()
        {
#ifdef ARDUINO_ARCH_RP2040
            recursive_mutex_init(&_mutex);
#endif
        }

        void Logger::begin()
        {
#ifdef ARDUINO_ARCH_RP2040
            recursive_mutex_enter_blocking(&_mutex);
#endif
        }

        void Logger::end()
        {
#ifdef ARDUINO_ARCH_RP2040
            recursive_mutex_exit(&_mutex);
#endif
        }

        void Logger::color(uint8_t color)
        {
            STATE_BY_CORE(_color) = color;
        }

        std::string Logger::buildPrefix(const std::string prefix, const std::string id)
        {
            char buffer[OPENKNX_MAX_LOG_PREFIX_LENGTH];
            sprintf(buffer, "%s<%s>", prefix.c_str(), id.c_str());
            return std::string(buffer);
        }

        std::string Logger::buildPrefix(const std::string prefix, const int id)
        {
            char buffer[OPENKNX_MAX_LOG_PREFIX_LENGTH];
            sprintf(buffer, "%s<%i>", prefix.c_str(), id);
            return std::string(buffer);
        }

        void Logger::beforeLog()
        {
            begin();
            clearPreviouseLine();
            if (isColorSet())
                printColorCode();
            printCore();
        }
        void Logger::afterLog()
        {
            if (isColorSet())
                printColorCode(0);
            OPENKNX_LOGGER_DEVICE.println();
            printPrompt();
            end();
        }

        void Logger::log(const std::string message)
        {
            beforeLog();
            printMessage(message);
            afterLog();
        }

        void Logger::logWithPrefix(const std::string prefix, const std::string message)
        {
            beforeLog();
            printPrefix(prefix);
            printIndent();
            printMessage(message);
            afterLog();
        }

        void Logger::logWithPrefixAndValues(const std::string prefix, const std::string message, ...)
        {
            va_list values;
            va_start(values, message);
            logWithPrefixAndValues(prefix, message, values);
            va_end(values);
        }

        void Logger::logWithPrefixAndValues(const std::string prefix, const std::string message, va_list values)
        {
            beforeLog();
            printPrefix(prefix);
            printIndent();
            printMessage(message, values);
            afterLog();
        }

        void Logger::logWithValues(const std::string message, ...)
        {
            va_list values;
            va_start(values, message);
            logWithValues(message, values);
            va_end(values);
        }

        void Logger::logWithValues(const std::string message, va_list values)
        {
            beforeLog();
            printMessage(message, values);
            afterLog();
        }

        void Logger::logHex(const uint8_t* data, size_t size)
        {
            beforeLog();
            printHex(data, size);
            afterLog();
        }

        void Logger::logHexWithPrefix(const std::string prefix, const uint8_t* data, size_t size)
        {
            beforeLog();
            printPrefix(prefix);
            printIndent();
            printHex(data, size);
            afterLog();
        }

        void Logger::logMacroWrapper(uint8_t logColor, const std::string prefix, const std::string message, ...)
        {
            color(logColor);
            va_list values;
            va_start(values, message);
            logWithPrefixAndValues(prefix, message, values);
            va_end(values);
            color(0);
        }

        void Logger::logHexMacroWrapper(uint8_t logColor, const std::string prefix, const uint8_t* data, size_t size)
        {
            color(logColor);
            logHexWithPrefix(prefix, data, size);
            color(0);
        }

        bool Logger::isColorSet()
        {
            return STATE_BY_CORE(_color) != 0;
        }

        void Logger::printColorCode(uint8_t color)
        {
            OPENKNX_LOGGER_DEVICE.print("\x1B[");
            OPENKNX_LOGGER_DEVICE.print((int)color);
            OPENKNX_LOGGER_DEVICE.print("m");
        }

        void Logger::printColorCode()
        {
            printColorCode(STATE_BY_CORE(_color));
        }

        void Logger::printHex(const uint8_t* data, size_t size)
        {
            for (size_t i = 0; i < size; i++)
            {
                if (data[i] < 0x10)
                    OPENKNX_LOGGER_DEVICE.print("0");

                OPENKNX_LOGGER_DEVICE.print(data[i], HEX);
                OPENKNX_LOGGER_DEVICE.print(" ");
            }
        }

        void Logger::clearPreviouseLine()
        {
#ifndef OPENKNX_RTT
            while (_lastConsoleLen > 0)
            {
                _lastConsoleLen--;
                OPENKNX_LOGGER_DEVICE.print("\b");
            }
            OPENKNX_LOGGER_DEVICE.print("\33[K");
#endif
        }

        void Logger::printPrompt()
        {
#ifndef OPENKNX_RTT
            clearPreviouseLine();
            OPENKNX_LOGGER_DEVICE.print(openknx.console.prompt);
            _lastConsoleLen = strlen(openknx.console.prompt);
#endif
        }

        void Logger::printPrefix(const std::string prefix)
        {
            size_t prefixLen = MIN(strlen(prefix.c_str()), OPENKNX_MAX_LOG_PREFIX_LENGTH);
            for (size_t i = 0; i < (OPENKNX_MAX_LOG_PREFIX_LENGTH + 2); i++)
            {
                if (i < prefixLen)
                {
                    OPENKNX_LOGGER_DEVICE.print(prefix.c_str()[i]);
                }
                else if (i == prefixLen && prefixLen > 0)
                {
                    OPENKNX_LOGGER_DEVICE.print(":");
                }
                else
                {
                    OPENKNX_LOGGER_DEVICE.print(" ");
                }
            }
        }

        void Logger::printCore()
        {
#if defined(ARDUINO_ARCH_RP2040) && (defined(OPENKNX_DEBUG) || defined(OPENKNX_LOGGER_SHOWCORE))
            if (openknx.common.usesDualCore())
                OPENKNX_LOGGER_DEVICE.print(rp2040.cpuid() ? "_1> " : "0_> ");
#endif
        }

        void Logger::printMessage(const std::string message)
        {
            OPENKNX_LOGGER_DEVICE.print(message.c_str());
        }

        void Logger::printMessage(const std::string message, va_list values)
        {
            memset(_buffer, 0, OPENKNX_MAX_LOG_MESSAGE_LENGTH);
            uint16_t len = vsnprintf(_buffer, OPENKNX_MAX_LOG_MESSAGE_LENGTH, message.c_str(), values);
            OPENKNX_LOGGER_DEVICE.print(_buffer);
            if (len >= OPENKNX_MAX_LOG_MESSAGE_LENGTH)
                openknx.hardware.fatalError(FATAL_SYSTEM, "BufferOverflow: increase OPENKNX_MAX_LOG_MESSAGE_LENGTH");
        }

#if defined(OPENKNX_TRACE1) || defined(OPENKNX_TRACE2) || defined(OPENKNX_TRACE3) || defined(OPENKNX_TRACE4) || defined(OPENKNX_TRACE5)
        bool Logger::checkTrace(std::string prefix)
        {
            MatchState ms;
            ms.Target((char*)prefix.c_str());
#ifdef OPENKNX_TRACE1
            if (strlen(TRACE_STRINGIFY(OPENKNX_TRACE1)) > 0 && ms.MatchCount(TRACE_STRINGIFY(OPENKNX_TRACE1)) > 0)
                return true;
#endif
#ifdef OPENKNX_TRACE2
            if (strlen(TRACE_STRINGIFY(OPENKNX_TRACE2)) > 0 && ms.MatchCount(TRACE_STRINGIFY(OPENKNX_TRACE2)) > 0)
                return true;
#endif
#ifdef OPENKNX_TRACE3
            if (strlen(TRACE_STRINGIFY(OPENKNX_TRACE3)) > 0 && ms.MatchCount(TRACE_STRINGIFY(OPENKNX_TRACE3)) > 0)
                return true;
#endif
#ifdef OPENKNX_TRACE4
            if (strlen(TRACE_STRINGIFY(OPENKNX_TRACE4)) > 0 && ms.MatchCount(TRACE_STRINGIFY(OPENKNX_TRACE4)) > 0)
                return true;
#endif
#ifdef OPENKNX_TRACE5
            if (strlen(TRACE_STRINGIFY(OPENKNX_TRACE5)) > 0 && ms.MatchCount(TRACE_STRINGIFY(OPENKNX_TRACE5)))
                return true;
#endif

            return false;
        }
#endif

        void Logger::printIndent()
        {
            for (size_t i = 0; i < getIndent(); i++)
            {
                OPENKNX_LOGGER_DEVICE.print("  ");
            }
        }

        void Logger::indentUp()
        {
            if (getIndent() == 10)
            {
                logError("Logger", "Indent error!");
            }
            else
            {
                STATE_BY_CORE(_indent)
                ++;
            }
        }

        void Logger::indentDown()
        {
            if (getIndent() == 0)
            {
                logError("Logger", "Indent error!");
            }
            else
            {
                STATE_BY_CORE(_indent)
                --;
            }
        }

        void Logger::indent(uint8_t indent)
        {
            STATE_BY_CORE(_indent) = indent;
        }

        uint8_t Logger::getIndent()
        {
            return STATE_BY_CORE(_indent);
        }
    } // namespace Log
} // namespace OpenKNX