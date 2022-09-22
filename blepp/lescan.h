/*
 *
 *  blepp - Implementation of the Generic ATTribute Protocol
 *
 *  Copyright (C) 2013, 2014 Edward Rosten
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef INC_BLEPP_LESCAN_H_
#define INC_BLEPP_LESCAN_H_

#include <blepp/blestatemachine.h> // for UUID. FIXME mofo
#include <bluetooth/hci.h>
#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace BLEPP {
	enum class LeAdvertisingEventType {
		// Connectable undirected advertising
		// Broadcast; any device can connect or ask for more information
		ADV_IND = 0x00,

		// Connectable Directed
		// Targeted; a single known device that can only connect
		ADV_DIRECT_IND = 0x01,

		// Scannable Undirected
		// Purely informative broadcast; devices can ask for more information
		ADV_SCAN_IND = 0x02,

		// Non-Connectable Undirected
		// Purely informative broadcast; no device can connect or even ask for more information
		ADV_NONCONN_IND = 0x03,

		// Result coming back after a scan request
		SCAN_RSP = 0x04,
	};

	//Is this the best design. I'm not especially convinced.
	//It seems pretty wretched.
	struct AdvertisingResponse {
		std::string address;
		LeAdvertisingEventType type;
		int8_t rssi;

		struct Name {
			std::string name;
			bool complete;
		};

		struct Flags {
			bool LE_limited_discoverable = false;
			bool LE_general_discoverable = false;
			bool BR_EDR_unsupported = false;
			bool simultaneous_LE_BR_controller = false;
			bool simultaneous_LE_BR_host = false;
			std::vector<uint8_t> flag_data;

			Flags(std::vector<uint8_t> &&);
		};

		std::vector<UUID> UUIDs;
		bool uuid_16_bit_complete  = false;
		bool uuid_32_bit_complete  = false;
		bool uuid_128_bit_complete = false;
		
		std::optional<Name>  local_name;
		std::optional<Flags> flags;

		std::vector<std::vector<uint8_t>> manufacturer_specific_data;
		std::vector<std::vector<uint8_t>> service_data;
		std::vector<std::vector<uint8_t>> unparsed_data_with_types;
		std::vector<std::vector<uint8_t>> raw_packet;
	};

	/// Class for scanning for BLE devices.
	/// This must be run as root because it requires getting packets from the HCI.
	/// The HCI requires root since it has no permissions on setting filters, so
	/// anyone with an open HCI device can sniff all data.
	class HCIScanner {
		private:
			class FD {
				private:
					int fd = -1;

				public:
					inline operator int() const {
						return fd;
					}

					FD() = default;
					FD(int fd_): fd(fd_) {}

					inline void set(int fd_) {
						fd = fd_;
					}

					~FD() {
						if (fd != -1)
							::close(fd);
					}
			};

		public:
			enum class ScanType {
				Passive = 0x00,
				Active  = 0x01,
			};

			enum class FilterDuplicates {
				// Get all events
				Off,

				// Rely on hardware filtering only. Lower power draw, but can actually send
				// duplicates if the device's builtin list gets overwhelmed.
				Hardware,

				// Get all events from the device and filter them by hand.
				Software,

				// The best and worst of both worlds.
				Both,
			};

			/// Generic error exception class
			class Error: public std::runtime_error {
				public:
					Error(const std::string &why);
			};

			/// Thrown only if a read() is interrupted. Bother handling
			/// only if you have a non-terminating exception handler.
			class Interrupted: public Error {
				using Error::Error;
			};

			/// IO error of some sort. Probably fatal for any Bluetooth-
			/// based system. Or might be that the dongle was unplugged.
			class IOError: public Error {
				public:
					IOError(const std::string &why, int errno_val);
			};

			/// The HCI device spat out invalid data.
			/// This is not good. Almost certainly fatal.
			class HCIError: public Error {
				using Error::Error;
			};

			HCIScanner();
			HCIScanner(bool start);
			HCIScanner(bool start, FilterDuplicates, ScanType, const std::string &device = "");

			void start();
			void stop();

			/// get the file descriptor.
			/// Use with select(), poll() or whatever.
			int get_fd() const;

			~HCIScanner();

			/// Blocking call. Use select() on the FD if you don't want to block.
			/// This reads and parses the HCI packets.
			std::vector<AdvertisingResponse> get_advertisements();

			/// Parse an HCI advertising packet. There's probably not much
			/// reason to call this yourself.
			static std::vector<AdvertisingResponse> parse_packet(const std::vector<uint8_t> &);

		private:
			struct FilterEntry {
				const std::string mac_address;
				int type;

				explicit FilterEntry(const AdvertisingResponse &);

				bool operator<(const FilterEntry &) const;
			};

			bool hardware_filtering;
			bool software_filtering;
			ScanType scan_type;

			FD hci_fd;
			bool running = false;
			hci_filter old_filter;
			
			/// Read the HCI data, but don't parse it.
			std::vector<uint8_t> read_with_retry();
			std::set<FilterEntry> scanned_devices;
	};
}

#endif
