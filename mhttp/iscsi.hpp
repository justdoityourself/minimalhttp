/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include "tcpserver.hpp"
#include "client.hpp"

#include "d8u/string_switch.hpp"

#include "d8u/buffer.hpp"
#include "d8u/string.hpp"

namespace mhttp
{
	namespace iscsi
	{
		//This is a MINIMAL iSCSI implementation, not a full spec.
		//SEE iSCSI: The Universal Storage Connection
		//SEE Seagate SCSI Command Reference

		static constexpr bool iscsi_debug = false;
		using namespace d8u;
		using namespace d8u::buffer;

		enum iscsi_commands
		{
			iscsi_nop_out,
			iscsi_scsi_cmd,
			iscsi_scsi_mgmt,
			iscsi_login,
			iscsi_text,
			iscsi_send,
			iscsi_logout,

			iscsi_nop_in = 0x20,
			iscsi_scsi_reply,
			iscsi_scsi_mgmt_reply,
			iscsi_login_reply,
			iscsi_text_reply,
			iscsi_recv,
			iscsi_logout_reply,

			iscsi_ready = 0x31,
			iscsi_async,
			iscsi_error = 0x3f
		};

		enum iscsi_errors
		{
			iscsi_err_reserved = 0x01,
			iscsi_err_digest = 0x02,
			iscsi_err_snack = 0x03,
			iscsi_err_protocol = 0x04,
			iscsi_err_not_supported = 0x05,
			iscsi_err_immediate_reject = 0x06,
			iscsi_err_task_underway = 0x07,
			iscsi_err_invalid_ack = 0x08,
			iscsi_err_invalid_field = 0x09,
			iscsi_err_long_reject = 0x0A,
			iscsi_err_reset = 0x0B,
			iscsi_err_logout = 0x0C,
		};

		enum scsi_commands
		{
			scsi_ready = 0x0,
			scsi_inquiry = 0x12,
			scsi_sense6 = 0x1a,
			scsi_read6 = 0x08,
			scsi_read10 = 0x28,
			scsi_write10 = 0x2a,
			scsi_read16 = 0x88,
			scsi_report_luns = 0xA0,
			scsi_capacity10 = 0x25,
			scsi_sync_cache = 0x35,
			scsi_service_action = 0x9e,
		};

#pragma pack(push,1)

		struct _scsi_sense
		{
			uint8_t ResponseCode : 7 = 0x70;
			uint8_t Valid : 1 = 1;

			uint8_t SegmentNumber = 0;

			uint8_t SenseKey : 4 = 0;
			uint8_t Reserved : 1 = 0;
			uint8_t IncorrectLengthIndicator : 1 = 0;
			uint8_t EndOfMedia : 1 = 0;
			uint8_t Filemark : 1 = 0;

			uint8_t Information[4] = {};
			uint8_t AdditionalSenseLength = 0;
			uint8_t CommandSpecificInfo[4] = {};
			uint8_t AdditionalSenseCode = 0;
			uint8_t AdditionalSenseCodeQualifier = 0;
			uint8_t FRUCode = 0;
			uint8_t SenseKeySpecific[3] = {};

			const uint8_t* data() const { return (uint8_t*)this; }
			const uint8_t* begin() const { return data(); }
			const uint8_t* end() const { return data() + size(); }
			size_t size() const { return sizeof(_scsi_sense); }
		};

		struct _scsi_inquiry
		{
			uint8_t DeviceType : 5 = 0;          // Disk
			uint8_t DeviceTypeQualifier : 3 = 0; // Connected;

			uint8_t DeviceTypeModifier : 7 = 0;  // Used only in SCSI-1
			uint8_t RemovableMedia : 1 = 0;      // Not Removable

			uint8_t ANSIVersion : 3 = 5;         // SCSI3
			uint8_t ECMAVersion : 3 = 0;         // !ECMA compliant
			uint8_t ISOVersion : 2 = 0;          // !ISO compliant

			uint8_t ResponseDataFormat : 4 = 2;  // SCSI3
			uint8_t Reserved1 : 2 = 0;
			uint8_t TerminateTask : 1 = 0;       // !Support TERMINATE I/O PROCESS message
			uint8_t AENC : 1 = 0;            

			uint8_t AdditionalLength = sizeof(_scsi_inquiry) - 4;
			uint8_t Reserved2[2] = {};

			uint8_t SoftReset : 1 = 0;           // 0 = Responds to RESET condition with hard RESET. 1 = Soft RESET.
			uint8_t CommandQueue : 1 = 1;        // 1 = Supports tagged command queuing for this logical unit.
			uint8_t Reserved3 : 1 = 0;
			uint8_t LinkedCommands : 1 = 0;      // 1 = Supports linked commands for this logical unit.
			uint8_t Synchronous : 1 = 0;         // 1 = Supports synchronous transfer.
			uint8_t Wide16Bit : 1 = 0;           // 1 = Supports 16-bit wide transfers
			uint8_t Wide32Bit : 1 = 0;           // 1 = Supports 32-bit wide transfers.
			uint8_t RelativeAddressing : 1 = 0;  // Supports relative addressing mode for this logical unit.

			uint8_t VendorId[8] = "_d8data";             
			uint8_t ProductId[16] = "dataworks_mhttp";
			uint8_t ProductRevisionLevel[4] = "420";

			uint64_t sn = 0;
			std::array<uint8_t, 14> reserved2 = {};
			std::array<uint16_t, 8> versions = { swap_endian < uint16_t>(0x0960) /*ISCSI*/,0,0,0,0,0,0,0 };
			std::array<uint8_t, 22> reserved3 = {};

			const uint8_t* data() const { return (uint8_t*)this; }
			const uint8_t* begin() const { return data(); }
			const uint8_t* end() const{ return data() + size(); }
			size_t size() const { return sizeof(_scsi_inquiry); }
		};

		struct scsi_capacity16
		{
			uint64_t lba;
			uint32_t block_size;

			std::array<uint8_t, 20> other;

			const uint8_t* data() const { return (uint8_t*)this; }
			const uint8_t* begin() const { return data(); }
			const uint8_t* end() const { return data() + size(); }
			size_t size() const { return sizeof(scsi_capacity16); }
		};

		static constexpr size_t iscsi_header_size = 48;

		struct iscsi_header_layout
		{
			union
			{
				struct
				{
					uint8_t cmd : 6;
					uint8_t I : 1;
					uint8_t dot : 1;

					uint8_t custom1 : 7;
					uint8_t finished : 1;
					uint8_t custom2;
					uint8_t custom3;
					uint8_t ahsl;
					uint8_t data_length[3];
					uint16_t LUN[4];
					uint32_t task_tag;
					uint32_t custom32[7];
				} standard;

				struct
				{
					uint8_t cmd : 6;
					uint8_t I : 1;
					uint8_t dot : 1;

					uint8_t attributes : 3;
					uint8_t custom1 : 2;
					uint8_t write : 1;
					uint8_t read : 1;
					uint8_t finished : 1;

					uint8_t custom2;
					uint8_t custom3;
					uint8_t ahsl;
					uint8_t data_length[3];
					uint16_t LUN[4];
					uint32_t task_tag;
					uint32_t transfer_length;
					uint32_t custom8[2];

					union
					{
						struct
						{
							uint8_t code;
							uint8_t product : 1;
							uint8_t other1 : 3;
							uint8_t disable_block_descriptors : 1;
							uint8_t other2 : 3;
							uint8_t page;

							uint8_t undefined[13];
						} scsi_descriptor;

						struct
						{
							uint8_t code;

							uint8_t action : 5;
							uint8_t cdb : 3;

							uint32_t lba;

							uint8_t cbd2;
							uint16_t length;

							uint8_t control;

							uint8_t undefined[6];
						} block10;

						struct
						{
							uint8_t code;
							uint8_t action : 5;
							uint8_t cdb : 3;
							uint32_t lba;
							uint32_t cbd2;
							uint32_t length;

							uint8_t cbd3;
							uint8_t control;
						} block16;
						uint8_t descriptor[16];
					};
				} scsi_cmd;

				struct
				{
					uint8_t cmd : 6;
					uint8_t I : 1;
					uint8_t dot : 1;

					uint8_t status_reported : 1;
					uint8_t underflow : 1;
					uint8_t overflow : 1;
					uint8_t custom1 : 3;
					uint8_t ack : 1;
					uint8_t finished : 1;

					uint8_t custom2;
					uint8_t custom3;
					uint8_t ahsl;
					uint8_t data_length[3];
					uint16_t LUN[4];
					uint32_t task_tag;
					uint32_t transfer_length;
					uint32_t custom8[2];
					uint8_t descriptor[16];
				} scsi_reply;

				struct
				{
					uint8_t cmd : 6;
					uint8_t I : 1;
					uint8_t dot : 1;

					uint8_t next : 2;
					uint8_t stage : 2;
					uint8_t skip1 : 2;
					uint8_t _continue : 1;
					uint8_t transit : 1;

					uint8_t vmax;
					uint8_t vmin;

					uint8_t ahsl;
					uint8_t data_length[3];

					uint8_t session_id[6]; 					//BE 32 BE 16
					uint16_t tsih;

					uint32_t task_tag;
					uint32_t custom32[7];
				} login;

				std::array<uint8_t, iscsi_header_size> byte_view;
				std::array<uint16_t, iscsi_header_size / 2> short_view;
				std::array<uint32_t, iscsi_header_size / 4> int_view;
				std::array<uint64_t, iscsi_header_size / 8> long_view;
			};
		};

		static_assert(sizeof(scsi_capacity16) == 32);
		static_assert(sizeof(_scsi_inquiry) == 96);
		static_assert(sizeof(iscsi_header_layout) == 48);

		template < size_t L > std::string_view string_viewz(const char(&t)[L])
		{
			return std::string_view(t, L);
		}

		class ISCSIMessage
		{
			gsl::span<uint8_t> request;
		public:
			ISCSIMessage(gsl::span<uint8_t> _request)
				: request(_request)
			{
				static_assert(sizeof(iscsi_header_layout) == 48);
			}

			iscsi_header_layout to_struct()
			{
				return *((iscsi_header_layout*)request.data());
			}

			uint16_t LUN0() { return swap_endian<uint16_t>(*(uint16_t*)(request.data())); }
			uint16_t LUN1() { return swap_endian<uint16_t>(*(uint16_t*)(request.data() + 2)); }
			uint16_t LUN2() { return swap_endian<uint16_t>(*(uint16_t*)(request.data() + 4)); }
			uint16_t LUN3() { return swap_endian<uint16_t>(*(uint16_t*)(request.data() + 6)); }

			uint32_t TaskTag() { return swap_endian<uint32_t>(*(uint32_t*)(request.data() + 16)); }
			uint32_t TransferTag() { return swap_endian<uint32_t>(*(uint32_t*)(request.data() + 20)); }
			uint32_t CommandSN() { return swap_endian<uint32_t>(*(uint32_t*)(request.data() + 24)); }
			uint32_t StatusSN() { return swap_endian<uint32_t>(*(uint32_t*)(request.data() + 28)); }

			bool Immediate() { return (request[0] & 0x40) != 0; }
			bool Finished() { return (request[1] & 0x80) != 0; }

			uint8_t Command() { return request[0] & 0x3F; }
			uint8_t AHSL() { return request[4]; }

			gsl::span<uint8_t> CommandHeader() { return gsl::span<uint8_t>(request.data() + 1, (size_t)3); }
			gsl::span<uint8_t> MessageDetails() { return gsl::span<uint8_t>(request.data() + 20, (size_t)28); }
			gsl::span<uint8_t> Data() { return gsl::span<uint8_t>(request.data() + 48, (size_t)(request[5] << 16 | request[6] << 8 | request[7])); }

			size_t Length()
			{
				size_t size = iscsi_header_size + AHSL() + Data().size();
				size += 4 - size % 4;
				return size;
			}

			template < typename T > size_t ReplyLength(const T& t)
			{
				size_t size = iscsi_header_size + AHSL() + t.size();
				size += 4 - size % 4;
				return size;
			}

			/*template < typename T > std::vector<uint8_t> Response(const T& t)
			{
				std::vector<uint8_t> result(ReplyLength(t));

				result[0] = Command();
				if (Immediate())
					result[0] |= 0x40;

				std::copy(CommandHeader().begin(), CommandHeader().end(), result.begin() + 1);

				if (Finished())
					result[1] |= 0x80;

				result[4] = AHSL();

				uint32_t sz = swap_endian<uint32_t>(t.size());
				std::copy(((uint8_t*)&sz), ((uint8_t*)&sz) + 3, result.data() + 5);

				std::copy(request.begin() + 8, request.begin() + 48, result.begin() + 8);
				std::copy(t.begin(), t.end(), result.begin() + iscsi_header_size);

				return result;
			}*/
		};

		template < size_t S=16 > class ISCSIReply : public ISCSIMessage
		{
		public:

			ISCSIReply() : ISCSIMessage(header.byte_view)
			{
				std::fill(header.long_view.begin(), header.long_view.end(), 0);
			}

			template < typename ... T > ISCSIReply(const T& ... t) : ISCSIReply()
			{
				size_t n = 0;

				auto copy = [&](const auto& x)
				{
					std::copy(x.begin(), x.end(), data.begin() + n);
					n += x.size();
				};

				(copy(t), ...);			
			}

			size_t BufferLength(size_t sz)
			{
				if (!data.size())
					return iscsi_header_size;

				size_t size = iscsi_header_size + AHSL() + sz;

				if(size % 4) 
					size += 4 - size % 4;

				return size;
			}

			std::vector<uint8_t> ToBuffer(size_t sz=0)
			{
				std::vector<uint8_t> result(BufferLength(sz));

				uint32_t szm = swap_endian<uint32_t>((uint32_t)sz);
				std::copy(((uint8_t*)&szm)+1, ((uint8_t*)&szm) + 4, header.standard.data_length);

				std::copy((uint8_t*)& header, ((uint8_t*)&header) + result.size(), result.begin());

				return result;
			}

			iscsi_header_layout header;
			std::array<uint8_t, S> data = { };
		};

		struct basic_disk
		{
			bool read_only = true;
			uint64_t blocks = 0;
			uint32_t block_size = 512;
			std::string target_name = "MinimalHTTPDefault";
		};

		class ISCSIConnection : public sock_t
		{
		public:
			ISCSIConnection() {}

			void Reject(uint8_t error)
			{
				ISCSIReply<> reply;
				reply.header.standard.custom1 = error;
				reply.header.standard.cmd = iscsi_error;
				reply.header.standard.finished = 1;
				reply.header.standard.task_tag = -1;
			}

			static constexpr uint32_t queue_size = 64;

			bool online = false;

			bool seq_order = true;
			bool pdu_order = true;
			bool immediate = true;
			bool r2t = true;

			uint32_t max_recv = 1024 * 1024;
			uint32_t max_con = 16;
			uint32_t max_burst;
			uint32_t first_burst;
			uint32_t wait_time;
			uint32_t retain_time;
			uint32_t outstanding_r2t;
			uint32_t recovery_level;


			uint32_t sn = 0;
			uint32_t cmd_sn = 1;

			std::string header_digest;
			std::string client_name;
			std::string session_type;
			std::string target_name;
			std::string auth_method;

			uint16_t CID = 1;
			uint32_t task_tag = 1;

			struct Transfer
			{
				uint64_t offset;
				uint32_t length;
				uint32_t current;
			};

			static constexpr uint32_t max_transfers = 128;
			std::atomic<uint32_t> current_transfer = 0;
			Transfer transfers[max_transfers];

			const basic_disk* disk = nullptr;
		};

#pragma pack(pop)

		template < typename LOGIN, typename LOGOUT, typename READ, typename WRITE, typename FLUSH> bool ISCSIBaseCommands(ISCSIConnection& c, ISCSIMessage msg, LOGIN on_login, LOGOUT on_logout, READ on_read, WRITE on_write, FLUSH on_flush, const basic_disk & _bd = basic_disk())
		{
			auto s = msg.to_struct();
			auto d = msg.Data();

			//if (!s.standard.finished)
			//	std::cout << "todo";

			auto send_scsi_reply = [&](auto&& reply,size_t sz, int cmd = iscsi_recv, bool status = true, bool flow=true, bool finished=true)
			{
				reply.header.standard.cmd = cmd;
				reply.header.standard.finished = (finished) ? 1 : 0;
				reply.header.standard.task_tag = s.standard.task_tag;
				reply.header.scsi_reply.status_reported = (status&&finished) ? 1 : 0;

				auto max = (size_t)swap_endian<uint32_t>(s.scsi_cmd.transfer_length);

				if (flow)
				{
					if (max > sz)
					{
						reply.header.scsi_reply.underflow = true;
						reply.header.standard.custom32[6] = swap_endian<uint32_t>((uint32_t)(max - sz));
					}
					else if (max < sz)
					{
						reply.header.scsi_reply.overflow = true;
						reply.header.standard.custom32[6] = swap_endian<uint32_t>((uint32_t)(sz - max));
						sz = max;
					}
				}

				reply.header.standard.custom32[1] = swap_endian<uint32_t>(c.sn++);
				reply.header.standard.custom32[2] = swap_endian<uint32_t>(c.cmd_sn);
				reply.header.standard.custom32[3] = swap_endian<uint32_t>(c.cmd_sn++ + c.queue_size);

				auto buffer = reply.ToBuffer(sz);
				if(iscsi_debug) std::cout << "Write: " << d8u::util::to_hex(buffer) << std::endl;
				c.AsyncWrite(std::move(buffer));
			};

			auto send_sense = [&](const auto & page)
			{
				if(!s.scsi_cmd.scsi_descriptor.disable_block_descriptors)
					send_scsi_reply(ISCSIReply<256>(std::array<uint8_t, 4>{(uint8_t)(11 + page.size()), 0, (uint8_t)(((c.disk->read_only) ? 0x80 : 0) | 0x10/*DPO FUA*/), 8},
													t_buffer<uint8_t>(std::array<uint32_t, 2>{0, swap_endian<uint32_t>(c.disk->block_size)}),
													page), 12+ page.size());
				else
					send_scsi_reply(ISCSIReply<256>(std::array<uint8_t, 4>{(uint8_t)(3 + page.size()),0, (uint8_t)(((c.disk->read_only) ? 0x80 : 0) | 0x10/*DPO FUA*/),0},
													page), 4 + page.size());
			};

			switch (msg.Command())
			{
			case iscsi_nop_out:
				std::cout << "TODO iscsi_nop_out" << std::endl;
				return false;
			case iscsi_recv:
				std::cout << "TODO iscsi_recv" << std::endl;
				return false;
			case iscsi_send:
			{
				if (s.standard.custom32[0] >= c.max_transfers)
					throw std::runtime_error("Bad Transfer ID");

				auto& handle = c.transfers[s.standard.custom32[0]];
				handle.current += (uint32_t)d.size() / c.disk->block_size;

				if (handle.current == handle.length)
					send_scsi_reply(ISCSIReply<>(), 0, iscsi_scsi_reply, false, false);

				auto offset = swap_endian<uint32_t>(s.standard.custom32[5]) / c.disk->block_size;

				on_write(c,handle.offset + offset, (uint32_t)d.size() / c.disk->block_size, d);

				return true;
			}
			case iscsi_scsi_cmd:
				switch (s.scsi_cmd.scsi_descriptor.code)
				{
				default:
					std::cout << "SCSI command not supported " << std::endl;
					return false;
				case scsi_ready:
					send_scsi_reply(ISCSIReply<>(), 0, iscsi_scsi_reply, false);
					return true;
				case scsi_write10:
				{
					constexpr size_t max_target = 1024 * 1024;
					auto tl = swap_endian<uint32_t>(s.scsi_cmd.transfer_length);
					if (s.scsi_cmd.write && tl > d.size())
					{
						auto rem = tl - d.size();
						auto count = (size_t)(rem / max_target + (rem % max_target) ? 1 : 0);

						auto transfer = c.current_transfer++ % c.max_transfers; //Overflow of transfer blocks possible with a rouge initiator, data corruption will ensue, but process will not be effected.
						auto& handle = c.transfers[transfer];

						handle.offset = (uint64_t)swap_endian<uint32_t>(s.scsi_cmd.block10.lba);
						handle.length = (uint32_t)swap_endian<uint16_t>(s.scsi_cmd.block10.length);
						handle.current = (uint32_t)d.size()/c.disk->block_size;

						for (size_t i = 0; i < count; i++)
						{
							ISCSIReply<> r;
							r.header.standard.custom32[0] = transfer; //Transfer Tag
							r.header.standard.custom32[4] = swap_endian<uint32_t>((uint32_t)i);
							r.header.standard.custom32[5] = swap_endian<uint32_t>((uint32_t)(d.size() + max_target * i));
							r.header.standard.custom32[6] = swap_endian<uint32_t>(((i+1) == count) ? rem % max_target : max_target);

							send_scsi_reply(r, 0, iscsi_ready, false, false);
						}

						on_write(c,(uint64_t)swap_endian<uint32_t>(s.scsi_cmd.block10.lba), (uint32_t)swap_endian<uint16_t>(s.scsi_cmd.block10.length), d);
					}
					else
					{
						send_scsi_reply(ISCSIReply<>(), 0, iscsi_scsi_reply, false, false);
						on_write(c,(uint64_t)swap_endian<uint32_t>(s.scsi_cmd.block10.lba), (uint32_t)swap_endian<uint16_t>(s.scsi_cmd.block10.length), d);
					}

					return true;
				}
				case scsi_read10:
				{
					auto count = (uint32_t)swap_endian<uint16_t>(s.scsi_cmd.block10.length);
					auto offset = (uint64_t)swap_endian<uint32_t>(s.scsi_cmd.block10.lba);
					if (count * c.disk->block_size > c.max_recv)
					{
						auto max = c.max_recv / c.disk->block_size;

						uint32_t itr = 0;
						while (count)
						{
							uint32_t current = count;
							if (current > max)
								current = max;

							ISCSIReply<0> reply(std::array<uint8_t, 0>{});
							reply.header.standard.custom32[4] = swap_endian<uint32_t>(itr);
							reply.header.standard.custom32[5] = swap_endian<uint32_t>(itr++ * c.max_recv);

							send_scsi_reply(reply, current * c.disk->block_size, iscsi_recv, true, false, current==count);

							on_read(c,offset, current, [&](uint64_t sector, uint32_t count, gsl::span<uint8_t> data)
							{
								std::vector<uint8_t> dup_buffer(data.size());
								std::copy(data.begin(), data.end(), dup_buffer.begin());
								c.AsyncWrite(std::move(dup_buffer));
							});

							offset += current;
							count -= current;
						}
					}
					else
					{
						send_scsi_reply(ISCSIReply<0>(std::array<uint8_t, 0>{}), count * c.disk->block_size, iscsi_recv, true, false);
						on_read(c,offset, (uint32_t)count, [&](uint64_t sector, uint32_t count, gsl::span<uint8_t> data)
						{
							std::vector<uint8_t> dup_buffer(data.size());
							std::copy(data.begin(), data.end(), dup_buffer.begin());
							c.AsyncWrite(std::move(dup_buffer));
						});
					}

					return true;
				}
				case scsi_sense6:

					switch (s.scsi_cmd.scsi_descriptor.page)
					{
					default:
						send_sense(std::array<uint8_t, 0>{});
						return true;
					case 0x08: //Cache
						send_sense(std::array<uint8_t, 20>{0x08, 18, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0});
						return true;
					case 0x0a: //Control
						std::cout << "missing sense control" << std::endl;
						return false;
					case 0x1a: //Power
						std::cout << "missing sense power" << std::endl;
						return false;
					case 0x1c: //Exceptions
						send_sense(std::array<uint8_t, 12>{0x1c, 10, 0, 0, 0, 0, 0, 0, 0, 0,0,0});
						return true;
					case 0x3f: //All Pages
						send_sense(std::array<uint8_t, 32>{0x08, 18, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1c, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
						return true;
					}

					send_scsi_reply(ISCSIReply<sizeof(_scsi_sense)+4>(_scsi_sense()), sizeof(_scsi_sense));
					return true;
				case scsi_service_action:
					switch (s.scsi_cmd.block16.action)
					{
					default:
						std::cout << "service action not supported " << std::endl;
						return false;
					case 0x10: //Capacity
						send_scsi_reply(ISCSIReply<32>(scsi_capacity16{ swap_endian<uint64_t>(c.disk->blocks-1),swap_endian<uint32_t>(c.disk->block_size),{} }), 32);
						return true;
					}
					return true;
				case scsi_capacity10:
					send_scsi_reply(ISCSIReply<8>(t_buffer<uint8_t>(std::array<uint32_t, 2>{swap_endian<uint32_t>((uint32_t)c.disk->blocks-1), swap_endian<uint32_t>(c.disk->block_size)})), 8);
					return true;
				case scsi_sync_cache:
					on_flush(c);
					send_scsi_reply(ISCSIReply<>(), 0, iscsi_scsi_reply, false);
					return true;
				case scsi_inquiry:
					if (!s.scsi_cmd.scsi_descriptor.product)
					{
						send_scsi_reply(ISCSIReply<sizeof(_scsi_inquiry)+4>(_scsi_inquiry()), sizeof(_scsi_inquiry));
						return true;
					}

					switch (s.scsi_cmd.scsi_descriptor.page)
					{
					default:
						std::cout << "Unknown page " << s.scsi_cmd.scsi_descriptor.page << std::endl;
						return false;
					case 0: //Supported Pages
						send_scsi_reply(ISCSIReply<8>(std::array<uint8_t, 8>{0, 0, 0, 3, 0, 0x80, 0x83, 0}), 7);
						return true;
					case 0x80: //SN
						send_scsi_reply(ISCSIReply<12>(std::array<uint8_t, 12>{0, 0x80, 0, 8, '0', '0', '0', '0', '0', '0', '0', '0'}), 12);
						return true;
					case 0x83: //DeviceID
						send_scsi_reply(ISCSIReply<256>(std::array<uint8_t, 20>{0, 0x83, 0, (uint8_t)(16 + c.disk->target_name.size()),
																					1/*Binary*/, 3 /*NAA*/,0,8 /*u16 len*/, 0x20, 0, 0, 0,5,0,0,0,
																					0x50 /*ISCSI*/ | 2 /*string*/, 0x20 /*Association*/ | 8 /*Target Name*/, 0, (uint8_t)c.disk->target_name.size()},c.disk->target_name), 20+c.disk->target_name.size());
						return true;
					case 0xB0: //Limits
						return false;
					case 0xB1: //Characteristics
						return false;
					}
				case scsi_report_luns:
					send_scsi_reply(ISCSIReply<16>(std::array<uint8_t, 16>{0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}),16);
					return true;
				}
				return true;
			case iscsi_scsi_mgmt:
				std::cout << "TODO iscsi_scsi_mgmt" << std::endl;
				return false;
			case iscsi_login:
				if (s.login._continue)
				{
					std::cout << "todo continue" << std::endl;
				}
				else
				{
					if (s.login.tsih == 0)
					{
						Helper line(d);
						while (line.size())
						{
							auto k = line.GetWord('='), v = line.GetWord('\0');
							switch (switch_t(k))
							{
							default:
								std::cout << "Unknown K/V Setting: " << std::string_view(k) << " / " << std::string_view(v) << std::endl;
								break;
							case switch_t("AuthMethod"):
								c.auth_method = v;
								break;
							case switch_t("HeaderDigest"):
								c.header_digest = v;
								break;
							case switch_t("DataDigest"):
								c.header_digest = v;
								break;
							case switch_t("InitiatorName"):
								c.client_name = v;
								break;
							case switch_t("SessionType"):
								c.session_type = v;
								break;
							case switch_t("TargetName"):
								c.target_name = v;
								break;
							case switch_t("MaxRecvDataSegmentLength"):
								c.max_recv = std::stoi(std::string(v));
								break;
							case switch_t("FirstBurstLength"):
								c.first_burst = std::stoi(std::string(v));
								break;
							case switch_t("MaxBurstLength"):
								c.max_burst = std::stoi(std::string(v));
								break;
							case switch_t("MaxConnections"):
								c.max_con = std::stoi(std::string(v));
								break;
							case switch_t("DefaultTime2Wait"):
								c.wait_time = std::stoi(std::string(v));
								break;
							case switch_t("MaxOutstandingR2T"):
								c.outstanding_r2t = std::stoi(std::string(v));
								break;
							case switch_t("ErrorRecoveryLevel"):
								c.recovery_level = std::stoi(std::string(v));
								break;
							case switch_t("DefaultTime2Retain"):
								c.retain_time = std::stoi(std::string(v));
								break;
							case switch_t("InitialR2T"):
								c.r2t = (std::string_view(v) == "Yes");
								break;
							case switch_t("ImmediateData"):
								c.immediate = (std::string_view(v) == "Yes");
								break;
							case switch_t("DataPDUInOrder"):
								c.pdu_order = (std::string_view(v) == "Yes");
								break;
							case switch_t("DataSequenceInOrder"):
								c.seq_order = (std::string_view(v) == "Yes");
								break;
							}
						}

						size_t sz = 0;
						ISCSIReply<1024> reply;

						if (s.login.stage == 0)
							sz = join_fixed(reply.data, string_viewz("AuthMethod=None"), (c.target_name.size()) ? string_viewz("TargetPortalGroupTag=1") : std::string_view(""));
						
						else if (s.login.stage == 1)
						{		
							if(c.session_type == "Discovery")
								sz = join_fixed(reply.data, string_viewz("DataDigest=None"),
															std::string_view("DefaultTime2Wait="), std::to_string(c.wait_time), string_viewz(""),
															std::string_view("DefaultTime2Retain="), std::to_string(c.retain_time), string_viewz(""),
															std::string_view("ErrorRecoveryLevel="), std::to_string(c.recovery_level), string_viewz(""),
															std::string_view("MaxRecvDataSegmentLength="),std::to_string(c.max_recv),string_viewz(""),
															string_viewz("HeaderDigest=None"));
							else
								sz = join_fixed(reply.data, string_viewz("DataDigest=None"),
									std::string_view("DefaultTime2Wait="), std::to_string(c.wait_time), string_viewz(""),
									std::string_view("DefaultTime2Retain="), std::to_string(c.retain_time), string_viewz(""),
									std::string_view("ErrorRecoveryLevel="), std::to_string(c.recovery_level), string_viewz(""),
									std::string_view("MaxRecvDataSegmentLength="), std::to_string(c.max_recv), string_viewz(""),
									std::string_view("MaxBurstLength="), std::to_string(c.max_burst), string_viewz(""),
									std::string_view("FirstBurstLength="), std::to_string(c.first_burst), string_viewz(""),
									std::string_view("MaxConnections="), std::to_string(c.max_con), string_viewz(""),
									std::string_view("InitialR2T="), (c.r2t) ? string_viewz("Yes") : string_viewz("No"),
									std::string_view("ImmediateData="), (c.immediate) ? string_viewz("Yes") : string_viewz("No"),
									std::string_view("MaxOutstandingR2T="), (c.outstanding_r2t) ? string_viewz("Yes") : string_viewz("No"),
									std::string_view("DataPDUInOrder="), (c.pdu_order) ? string_viewz("Yes") : string_viewz("No"),
									std::string_view("DataSequenceInOrder="), (c.seq_order) ? string_viewz("Yes") : string_viewz("No"),
									string_viewz("HeaderDigest=None"));
						}

						reply.header.login.cmd = iscsi_login_reply;
						reply.header.login.transit = 1;
						reply.header.login._continue = 0;
						reply.header.login.stage = s.login.stage;
						reply.header.login.next = s.login.next;
						reply.header.login.vmin = s.login.vmin;
						reply.header.login.vmax = s.login.vmax;
						std::copy(s.login.session_id, s.login.session_id + 6, reply.header.login.session_id);
						reply.header.login.tsih = swap_endian<uint16_t>(3);
						reply.header.login.task_tag = s.login.task_tag;

						reply.header.login.custom32[1] = swap_endian<uint32_t>(c.sn++);
						reply.header.login.custom32[2] = swap_endian<uint32_t>(c.cmd_sn);
						reply.header.login.custom32[3] = swap_endian<uint32_t>(c.cmd_sn + c.queue_size);

						if (s.login.next == 3)
						{
							c.online = true;
							c.cmd_sn++;

							on_login(c);
							if (!c.disk)
								c.disk = &_bd;
						}

						auto buffer = reply.ToBuffer(sz);
						if (iscsi_debug) std::cout << "Write: " << d8u::util::to_hex(buffer) << std::endl;
						c.AsyncWrite(std::move(buffer));
					}
					else
					{
						std::cout << "TODO tsih" << std::endl;
			 		}
				}
				return true;
			case iscsi_text:
			{
				ISCSIReply<1024> reply;
				size_t sz = 0;

				Helper line(d);
				while (line.size())
				{
					auto k = line.GetWord('='), v = line.GetWord('\0');
					switch (switch_t(k))
					{
					default:
						std::cout << "Unknown K/V Text Command: " << k << " / " << v << std::endl;
						break;
					case switch_t("SendTargets"):
						sz = join_fixed(reply.data, std::string_view("TargetName="),_bd.target_name,string_viewz(""));
						break;
					}
				}

				reply.header.standard.cmd = iscsi_text_reply;
				reply.header.standard.finished = s.standard.finished;
				reply.header.standard.task_tag = s.standard.task_tag;		

				reply.header.login.custom32[1] = swap_endian<uint32_t>(c.sn++);
				reply.header.login.custom32[2] = swap_endian<uint32_t>(c.cmd_sn);
				reply.header.login.custom32[3] = swap_endian<uint32_t>(c.cmd_sn++ + c.queue_size);

				auto buffer = reply.ToBuffer(sz);
				if (iscsi_debug) std::cout << "Write: " << d8u::util::to_hex(buffer) << std::endl;
				c.AsyncWrite(std::move(buffer));

				return true;
			}
			case iscsi_logout:
			{
				ISCSIReply<0> reply;

				reply.header.standard.cmd = iscsi_logout_reply;

				reply.header.standard.finished = s.standard.finished;
				reply.header.standard.task_tag = s.standard.task_tag;

				reply.header.login.custom32[1] = swap_endian<uint32_t>(c.sn++);
				reply.header.login.custom32[2] = swap_endian<uint32_t>(c.cmd_sn);
				reply.header.login.custom32[3] = swap_endian<uint32_t>(c.cmd_sn++ + c.queue_size);

				auto buffer = reply.ToBuffer();
				if (iscsi_debug) std::cout << "Write: " << d8u::util::to_hex(buffer) << std::endl;
				c.AsyncWrite(std::move(buffer));

				on_logout(c);

				return true;
			}		
			case iscsi_nop_in:
				std::cout << "TODO iscsi_nop_in" << std::endl;
				return false;
			case iscsi_scsi_reply:
				std::cout << "TODO iscsi_scsi_reply" << std::endl;
				return false;
			case iscsi_scsi_mgmt_reply:
				std::cout << "TODO iscsi_scsi_mgmt_reply" << std::endl;
				return false;
			case iscsi_login_reply:
				std::cout << "TODO iscsi_login_reply" << std::endl;
				return false;
			case iscsi_text_reply:
				std::cout << "TODO iscsi_text_reply" << std::endl;
				return false;
			case iscsi_logout_reply:
				std::cout << "TODO iscsi_logout_reply" << std::endl;
				return false;
			case iscsi_ready:
				std::cout << "TODO iscsi_ready" << std::endl;
				return false;
			case iscsi_async:
				std::cout << "TODO iscsi_async" << std::endl;
				return false;
			case iscsi_error:
				std::cout << "TODO iscsi_error" << std::endl;
				return false;
			default:
				std::cout << "Unknown cmd " << std::to_string(msg.Command()) << std::endl;
				return false;
			}

			return false;
		}

		template < typename LOGIN, typename LOGOUT, typename READ, typename WRITE, typename FLUSH > auto make_iscsi_server(std::string_view port, LOGIN && on_login, LOGOUT && on_logout,READ &&on_read, WRITE &&on_write, FLUSH&& on_flush, const basic_disk& bd = basic_disk(), size_t threads = 1, bool mplex = false)
		{
			auto on_connect = [](const auto& c) { return std::make_pair(true, true); };

			auto on_disconnect = [](const auto& c){};

			auto on_error = [](const auto& c) {};
			auto _on_write = [](const auto& mc, const auto& c) {};

			return TcpServer<ISCSIConnection>((uint16_t)std::stoi(port.data()), ConnectionType::iscsi, on_connect, on_disconnect, [bd, on_logout = std::move(on_logout), on_login = std::move(on_login), on_read = std::move(on_read), on_write = std::move(on_write), on_flush = std::move(on_flush)](auto* _server, auto* _client, auto&& _request, auto body, auto* mplex) mutable
			{
				auto server = (TcpServer<ISCSIConnection>*)_server;
				auto& client = *(ISCSIConnection*)_client;

				if (iscsi_debug) std::cout << "Read: " << d8u::util::to_hex(body) << std::endl;

				try
				{
					if (!ISCSIBaseCommands(client, body,on_login,on_logout,on_read,on_write, on_flush,bd))
						throw std::runtime_error("Unsupported command");
				}
				catch (...)
				{
					client.Reject(iscsi_err_protocol);
				}
			}, on_error, _on_write, mplex, { threads });
		}

		using on_iscsi_read_ready = std::function < void(uint64_t, uint32_t, gsl::span<uint8_t>)>;

		using on_iscsi_read = std::function < void(ISCSIConnection& , uint64_t, uint32_t, on_iscsi_read_ready)>;
		using on_iscsi_write = std::function < void(ISCSIConnection& ,uint64_t,uint32_t,gsl::span<uint8_t>)>;
		using on_iscsi_flush = std::function < void(ISCSIConnection&)>;
		using on_iscsi_login = std::function < void(ISCSIConnection&)>;
		using on_iscsi_logout = std::function < void(ISCSIConnection&)>;

		class iScsiServer : public TcpServer<ISCSIConnection>
		{
			on_iscsi_read on_read;
			on_iscsi_write on_write;
			on_iscsi_flush on_flush;
			on_iscsi_login on_login;
			on_iscsi_logout on_logout;

		public:
			iScsiServer(on_iscsi_login _on_login, on_iscsi_logout _on_logout, on_iscsi_read _on_read, on_iscsi_write _on_write, on_iscsi_flush _on_flush, const basic_disk& bd = basic_disk(),TcpServer::Options o = TcpServer::Options())
				: on_read(_on_read)
				, on_write(_on_write)
				, on_flush(_on_flush)
				, on_login(_on_login)
				, on_logout(_on_logout)
				, TcpServer([&](auto* _server, auto* _client, auto&& _request, auto body, auto* mplex)
			{
				auto server = (TcpServer<ISCSIConnection>*)_server;
				auto& client = *(ISCSIConnection*)_client;

				try
				{
					if (!ISCSIBaseCommands(client, body, on_login, on_logout, on_read, on_write, on_flush, bd))
						throw std::runtime_error("Unsupported command");
				}
				catch (...)
				{
					client.Reject(iscsi_err_protocol);
				}
			}, [&](auto & c)
			{
				on_logout(*((ISCSIConnection*)&c));
			}, o) { }

			iScsiServer(std::string_view port, on_iscsi_login _on_login, on_iscsi_logout _on_logout, on_iscsi_read _on_read, on_iscsi_write _on_write, on_iscsi_flush _on_flush,  const basic_disk& bd = basic_disk(), bool mplex = false, TcpServer::Options o = TcpServer::Options())
				: iScsiServer( _on_login,_on_logout, _on_read, _on_write, _on_flush, bd, o)
			{
				Open(port,"",mplex);
			}

			void Open(const std::string_view port, const std::string& options = "", bool mplex = false)
			{
				TcpServer::Open((uint16_t)std::stoi(port.data()), options, ConnectionType::iscsi, mplex);
			}
		};
	}
}