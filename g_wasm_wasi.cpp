/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

extern "C"
{
#include "g_main.h"

#include <wasm_export.h>
#include "g_wasm.h"
}

enum wasi_clockid_t : uint32_t
{
	CLOCKID_REALTIME                          =  0,
	CLOCKID_MONOTONIC                         =  1,
	CLOCKID_PROCESS_CPUTIME_ID                =  2,
	CLOCKID_THREAD_CPUTIME_ID                 =  3
};

enum wasi_errno_t : uint16_t
{
	ERRNO_SUCCESS                             =  0,
	ERRNO_2BIG                                =  1,
	ERRNO_ACCES                               =  2,
	ERRNO_ADDRINUSE                           =  3,
	ERRNO_ADDRNOTAVAIL                        =  4,
	ERRNO_AFNOSUPPORT                         =  5,
	ERRNO_AGAIN                               =  6,
	ERRNO_ALREADY                             =  7,
	ERRNO_BADF                                =  8,
	ERRNO_BADMSG                              =  9,
	ERRNO_BUSY                                =  10,
	ERRNO_CANCELED                            =  11,
	ERRNO_CHILD                               =  12,
	ERRNO_CONNABORTED                         =  13,
	ERRNO_CONNREFUSED                         =  14,
	ERRNO_CONNRESET                           =  15,
	ERRNO_DEADLK                              =  16,
	ERRNO_DESTADDRREQ                         =  17,
	ERRNO_DOM                                 =  18,
	ERRNO_DQUOT                               =  19,
	ERRNO_EXIST                               =  20,
	ERRNO_FAULT                               =  21,
	ERRNO_FBIG                                =  22,
	ERRNO_HOSTUNREACH                         =  23,
	ERRNO_IDRM                                =  24,
	ERRNO_ILSEQ                               =  25,
	ERRNO_INPROGRESS                          =  26,
	ERRNO_INTR                                =  27,
	ERRNO_INVAL                               =  28,
	ERRNO_IO                                  =  29,
	ERRNO_ISCONN                              =  30,
	ERRNO_ISDIR                               =  31,
	ERRNO_LOOP                                =  32,
	ERRNO_MFILE                               =  33,
	ERRNO_MLINK                               =  34,
	ERRNO_MSGSIZE                             =  35,
	ERRNO_MULTIHOP                            =  36,
	ERRNO_NAMETOOLONG                         =  37,
	ERRNO_NETDOWN                             =  38,
	ERRNO_NETRESET                            =  39,
	ERRNO_NETUNREACH                          =  40,
	ERRNO_NFILE                               =  41,
	ERRNO_NOBUFS                              =  42,
	ERRNO_NODEV                               =  43,
	ERRNO_NOENT                               =  44,
	ERRNO_NOEXEC                              =  45,
	ERRNO_NOLCK                               =  46,
	ERRNO_NOLINK                              =  47,
	ERRNO_NOMEM                               =  48,
	ERRNO_NOMSG                               =  49,
	ERRNO_NOPROTOOPT                          =  50,
	ERRNO_NOSPC                               =  51,
	ERRNO_NOSYS                               =  52,
	ERRNO_NOTCONN                             =  53,
	ERRNO_NOTDIR                              =  54,
	ERRNO_NOTEMPTY                            =  55,
	ERRNO_NOTRECOVERABLE                      =  56,
	ERRNO_NOTSOCK                             =  57,
	ERRNO_NOTSUP                              =  58,
	ERRNO_NOTTY                               =  59,
	ERRNO_NXIO                                =  60,
	ERRNO_OVERFLOW                            =  61,
	ERRNO_OWNERDEAD                           =  62,
	ERRNO_PERM                                =  63,
	ERRNO_PIPE                                =  64,
	ERRNO_PROTO                               =  65,
	ERRNO_PROTONOSUPPORT                      =  66,
	ERRNO_PROTOTYPE                           =  67,
	ERRNO_RANGE                               =  68,
	ERRNO_ROFS                                =  69,
	ERRNO_SPIPE                               =  70,
	ERRNO_SRCH                                =  71,
	ERRNO_STALE                               =  72,
	ERRNO_TIMEDOUT                            =  73,
	ERRNO_TXTBSY                              =  74,
	ERRNO_XDEV                                =  75,
	ERRNO_NOTCAPABLE                          =  76,
};

enum wasi_rights_t : uint64_t
{
	RIGHTS_FD_DATASYNC                        =  0x00000001ll,
	RIGHTS_FD_READ                            =  0x00000002ll,
	RIGHTS_FD_SEEK                            =  0x00000004ll,
	RIGHTS_FD_FDSTAT_SET_FLAGS                =  0x00000008ll,
	RIGHTS_FD_SYNC                            =  0x00000010ll,
	RIGHTS_FD_TELL                            =  0x00000020ll,
	RIGHTS_FD_WRITE                           =  0x00000040ll,
	RIGHTS_FD_ADVISE                          =  0x00000080ll,
	RIGHTS_FD_ALLOCATE                        =  0x00000100ll,
	RIGHTS_PATH_CREATE_DIRECTORY              =  0x00000200ll,
	RIGHTS_PATH_CREATE_FILE                   =  0x00000400ll,
	RIGHTS_PATH_LINK_SOURCE                   =  0x00000800ll,
	RIGHTS_PATH_LINK_TARGET                   =  0x00001000ll,
	RIGHTS_PATH_OPEN                          =  0x00002000ll,
	RIGHTS_FD_READDIR                         =  0x00004000ll,
	RIGHTS_PATH_READLINK                      =  0x00008000ll,
	RIGHTS_PATH_RENAME_SOURCE                 =  0x00010000ll,
	RIGHTS_PATH_RENAME_TARGET                 =  0x00020000ll,
	RIGHTS_PATH_FILESTAT_GET                  =  0x00040000ll,
	RIGHTS_PATH_FILESTAT_SET_SIZE             =  0x00080000ll,
	RIGHTS_PATH_FILESTAT_SET_TIMES            =  0x00100000ll,
	RIGHTS_FD_FILESTAT_GET                    =  0x00200000ll,
	RIGHTS_FD_FILESTAT_SET_SIZE               =  0x00400000ll,
	RIGHTS_FD_FILESTAT_SET_TIMES              =  0x00800000ll,
	RIGHTS_PATH_SYMLINK                       =  0x01000000ll,
	RIGHTS_PATH_REMOVE_DIRECTORY              =  0x02000000ll,
	RIGHTS_PATH_UNLINK_FILE                   =  0x04000000ll,
	RIGHTS_POLL_FD_READWRITE                  =  0x08000000ll,
	RIGHTS_SOCK_SHUTDOWN                      =  0x10000000ll
};

enum wasi_whence_t : uint8_t
{
	WHENCE_SET                                =  0,
	WHENCE_CUR                                =  1,
	WHENCE_END                                =  2
};

enum wasi_filetype_t : uint8_t
{
	FILETYPE_UNKNOWN          =  0,
	FILETYPE_BLOCK_DEVICE     =  1,
	FILETYPE_CHARACTER_DEVICE =  2,
	FILETYPE_DIRECTORY        =  3,
	FILETYPE_REGULAR_FILE     =  4,
	FILETYPE_SOCKET_DGRAM     =  5,
	FILETYPE_SOCKET_STREAM    =  6,
	FILETYPE_SYMBOLIC_LINK    =  7
};

enum wasi_advice_t : uint8_t
{
	ADVICE_NORMAL                             =  0,
	ADVICE_SEQUENTIAL                         =  1,
	ADVICE_RANDOM                             =  2,
	ADVICE_WILLNEED                           =  3,
	ADVICE_DONTNEED                           =  4,
	ADVICE_NOREUSE                            =  5
};

enum wasi_fdflags_t : uint16_t
{
	FDFLAGS_APPEND                            =  0x0001,
	FDFLAGS_DSYNC                             =  0x0002,
	FDFLAGS_NONBLOCK                          =  0x0004,
	FDFLAGS_RSYNC                             =  0x0008,
	FDFLAGS_SYNC                              =  0x0010
};

enum wasi_fstflags_t : uint16_t
{
	FSTFLAGS_ATIM                             =  0x0001,
	FSTFLAGS_ATIM_NOW                         =  0x0002,
	FSTFLAGS_MTIM                             =  0x0004,
	FSTFLAGS_MTIM_NOW                         =  0x0008
};

enum wasi_lookupflags_t : uint32_t
{
	LOOKUPFLAGS_SYMLINK_FOLLOW                =  0x0001
};

enum wasi_oflags_t : uint16_t
{
	OFLAGS_CREAT                              =  0x0001,
	OFLAGS_DIRECTORY                          =  0x0002,
	OFLAGS_EXCL                               =  0x0004,
	OFLAGS_TRUNC                              =  0x0008
};

enum wasi_eventtype_t : uint8_t
{
	EVENTTYPE_CLOCK                           =  0,
	EVENTTYPE_FD_READ                         =  1,
	EVENTTYPE_FD_WRITE                        =  2
};

enum wasi_eventrwflags_t : uint16_t
{
	EVENTRWFLAGS_FD_READWRITE_HANGUP          =  1
};

enum wasi_subclockflags_t : uint16_t
{
	SUBCLOCKFLAGS_SUBSCRIPTION_CLOCK_ABSTIME  =  1
};

enum wasi_signal_t : uint8_t
{
	SIGNAL_NONE                               =  0,
	SIGNAL_HUP                                =  1,
	SIGNAL_INT                                =  2,
	SIGNAL_QUIT                               =  3,
	SIGNAL_ILL                                =  4,
	SIGNAL_TRAP                               =  5,
	SIGNAL_ABRT                               =  6,
	SIGNAL_BUS                                =  7,
	SIGNAL_FPE                                =  8,
	SIGNAL_KILL                               =  9,
	SIGNAL_USR1                               =  10,
	SIGNAL_SEGV                               =  11,
	SIGNAL_USR2                               =  12,
	SIGNAL_PIPE                               =  13,
	SIGNAL_ALRM                               =  14,
	SIGNAL_TERM                               =  15,
	SIGNAL_CHLD                               =  16,
	SIGNAL_CONT                               =  17,
	SIGNAL_STOP                               =  18,
	SIGNAL_TSTP                               =  19,
	SIGNAL_TTIN                               =  20,
	SIGNAL_TTOU                               =  21,
	SIGNAL_URG                                =  22,
	SIGNAL_XCPU                               =  23,
	SIGNAL_XFSZ                               =  24,
	SIGNAL_VTALRM                             =  25,
	SIGNAL_PROF                               =  26,
	SIGNAL_WINCH                              =  27,
	SIGNAL_POLL                               =  28,
	SIGNAL_PWR                                =  29,
	SIGNAL_SYS                                =  30
};

enum wasi_riflags_t : uint16_t 
{
	RIFLAGS_RECV_PEEK                         =  0x0001,
	RIFLAGS_RECV_WAITALL                      =  0x0002
};

enum wasi_roflags_t : uint16_t 
{
	ROFLAGS_RECV_DATA_TRUNCATED               =  0x0001
};

enum wasi_siflags_t : uint16_t { };

enum wasi_sdflags_t : uint8_t
{
	SDFLAGS_RD                                =  0x0001,
	SDFLAGS_WR                                =  0x0002
};

enum wasi_preopentype_t : uint8_t
{
	PREOPENTYPE_DIR                           =  0
};

#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

struct fd
{
	bool preopen;
	wasi_filetype_t type;
	wasi_fdflags_t flags;
	wasi_rights_t rights_base;
	wasi_rights_t rights_inheriting;

	// for directory types
	fs::path	path;

	// for open files
	std::fstream stream;
};

static std::vector<fd> fds;

#define BUF_OFFSET(buf, t, o) \
	*((t *)(buf + o))

static int32_t q2_fd_prestat_get(wasm_exec_env_t, int32_t id, uint8_t *out_buffer)
{
	if (id >= fds.size())
		return ERRNO_BADF;

	auto &entry = fds[id];

	if (entry.preopen && entry.type == FILETYPE_DIRECTORY)
	{
		BUF_OFFSET(out_buffer, wasi_preopentype_t, 0) = PREOPENTYPE_DIR;
		BUF_OFFSET(out_buffer, uint32_t, 4) = entry.path.string().size();
		return ERRNO_SUCCESS;
	}

	return ERRNO_BADF;
}

static int32_t q2_fd_prestat_dir_name(wasm_exec_env_t, int32_t id, char *path, uint32_t path_len)
{
	if (id >= fds.size())
		return ERRNO_BADF;

	auto &entry = fds[id];

	if (entry.preopen && entry.type == FILETYPE_DIRECTORY)
	{
		if (path_len != entry.path.string().size())
			return ERRNO_BADF;

		strcpy(path, entry.path.string().c_str());
		return ERRNO_SUCCESS;
	}

	return ERRNO_BADF;
}

static int32_t q2_fd_fdstat_get(wasm_exec_env_t, int32_t id, uint8_t *out_buffer)
{
	if (id >= fds.size())
		return ERRNO_BADF;

	auto &entry = fds[id];

	if (entry.type == FILETYPE_DIRECTORY)
	{
		BUF_OFFSET(out_buffer, wasi_filetype_t, 0) = FILETYPE_DIRECTORY;
		BUF_OFFSET(out_buffer, wasi_fdflags_t, 4) = entry.flags;
		BUF_OFFSET(out_buffer, wasi_rights_t, 8) = entry.rights_base;
		BUF_OFFSET(out_buffer, wasi_rights_t, 16) = entry.rights_inheriting;
		return ERRNO_SUCCESS;
	}

	return ERRNO_BADF;
}

static int32_t q2_path_open(wasm_exec_env_t, int32_t id, int32_t dirflags_i, char *path_ptr, uint32_t path_len, int32_t oflags_i, int64_t rights_base_i, int64_t rights_inheriting_i, int32_t fdflags_i, int32_t *opened_fd_out)
{
	if (id >= fds.size())
		return ERRNO_BADF;

	auto &entry = fds[id];

	if (entry.type == FILETYPE_DIRECTORY)
	{
		wasi_lookupflags_t dirflags = (wasi_lookupflags_t) dirflags_i;
		wasi_oflags_t oflags = (wasi_oflags_t) oflags_i;
		wasi_rights_t rights_base = (wasi_rights_t) rights_base_i;
		wasi_rights_t rights_inheriting = (wasi_rights_t) rights_inheriting_i;
		wasi_fdflags_t fdflags = (wasi_fdflags_t) fdflags_i;

		fs::path path = entry.path / path_ptr;

		if (!(oflags & OFLAGS_CREAT))
		{
			if (!fs::exists(path))
				return ERRNO_NXIO;
			else if (!fs::is_regular_file(path))
				return ERRNO_NOTSUP;
		}
		
		if (oflags & OFLAGS_EXCL)
		{
			if (fs::exists(path))
				return ERRNO_EXIST;
			else if (!fs::is_regular_file(path))
				return ERRNO_NOTSUP;
		}

		std::fstream::openmode om = std::fstream::binary | std::fstream::in | std::fstream::out;

		if (oflags & OFLAGS_TRUNC)
			om |= std::fstream::trunc;

		if (fdflags & FDFLAGS_APPEND)
			om |= std::fstream::app;

		fds.push_back({
			.type = FILETYPE_REGULAR_FILE,
			.stream = std::fstream(path, om)
		});

		*opened_fd_out = fds.size() - 1;

		return ERRNO_SUCCESS;
	}

	return ERRNO_BADF;
}

static int32_t q2_fd_read(wasm_exec_env_t, int32_t id, uint8_t *iovs_ptr, uint32_t iovs_len, uint32_t *nread_out)
{
	if (id >= fds.size())
		return ERRNO_BADF;

	auto &entry = fds[id];

	if (entry.type == FILETYPE_REGULAR_FILE)
	{
		uint32_t nread = 0, offset = 0;

		for (uint32_t i = 0; i < iovs_len; i++)
		{
			void *ptr = wasm_addr_to_native(BUF_OFFSET(iovs_ptr, uint32_t, offset));
			offset += 4;

			uint32_t len = BUF_OFFSET(iovs_ptr, uint32_t, offset);
			offset += 4;

			entry.stream.read((char *) ptr, len);
			nread += entry.stream.gcount();
		}

		*nread_out = nread;
		return ERRNO_SUCCESS;
	}

	return ERRNO_BADF;
}

static int32_t q2_fd_write(wasm_exec_env_t, int32_t id, uint8_t *iovs_ptr, uint32_t iovs_len, uint32_t *nread_out)
{
	if (id >= fds.size())
		return ERRNO_BADF;

	auto &entry = fds[id];

	if (entry.type == FILETYPE_REGULAR_FILE)
	{
		uint32_t nread = 0, offset = 0;

		for (uint32_t i = 0; i < iovs_len; i++)
		{
			void *ptr = wasm_addr_to_native(BUF_OFFSET(iovs_ptr, uint32_t, offset));
			offset += 4;

			uint32_t len = BUF_OFFSET(iovs_ptr, uint32_t, offset);
			offset += 4;

			entry.stream.write((char *) ptr, len);
			nread += len;
		}

		*nread_out = nread;
		return ERRNO_SUCCESS;
	}

	return ERRNO_BADF;
}

static int32_t q2_fd_close(wasm_exec_env_t, int32_t id)
{
	if (id >= fds.size())
		return ERRNO_BADF;
	
	auto entry = fds.begin() + id;

	if ((*entry).preopen)
		return ERRNO_BADF;

	fds.erase(entry);

	return ERRNO_BADF;
}

#include <chrono>
namespace chrono = std::chrono;

static chrono::steady_clock monotonic;
static chrono::system_clock real_clock;

static int32_t q2_clock_time_get(wasm_exec_env_t, int32_t clock_id, uint64_t, uint64_t *stamp)
{
	switch (clock_id)
	{
	case CLOCKID_REALTIME:
		*stamp = monotonic.now().time_since_epoch().count();
		return ERRNO_SUCCESS;
	case CLOCKID_MONOTONIC:
		*stamp = real_clock.now().time_since_epoch().count();
		return ERRNO_SUCCESS;
	}

	return ERRNO_INVAL;
}

#define SYMBOL(name, sig) \
	{ #name, (void *) q2_ ## name, sig, nullptr }

static NativeSymbol native_symbols_libc_wasi[] = {
	//SYMBOL(args_get, "(**)i"),
	//SYMBOL(args_sizes_get, "(**)i"),
	//SYMBOL(clock_res_get, "(i*)i"),
	SYMBOL(clock_time_get, "(iI*)i"),
	//SYMBOL(environ_get, "(**)i"),
	//SYMBOL(environ_sizes_get, "(**)i"),
	SYMBOL(fd_prestat_get, "(i*)i"),
	SYMBOL(fd_prestat_dir_name, "(i*~)i"),
	SYMBOL(fd_close, "(i)i"),
	/*SYMBOL_F(fd_datasync, NOP, "(i)i"),
	SYMBOL_F(fd_pread, NOP, "(i*iI*)i"),
	SYMBOL_F(fd_pwrite, NOP, "(i*iI*)i"),*/
	SYMBOL(fd_read, "(i*i*)i"),
	/*SYMBOL_F(fd_renumber, NOP, "(ii)i"),
	SYMBOL_F(fd_seek, NOP, "(iIi*)i"),
	SYMBOL_F(fd_tell, NOP, "(i*)i"),*/
	SYMBOL(fd_fdstat_get, "(i*)i"),
	/*SYMBOL_F(fd_fdstat_set_flags, NOP, "(ii)i"),
	SYMBOL_F(fd_fdstat_set_rights, NOP, "(iII)i"),
	SYMBOL_F(fd_sync, NOP, "(i)i"),*/
	SYMBOL(fd_write, "(i*i*)i"),
	/*SYMBOL_F(fd_advise, NOP, "(iIIi)i"),
	SYMBOL_F(fd_allocate, NOP, "(iII)i"),
	SYMBOL_F(path_create_directory, NOP, "(i*~)i"),
	SYMBOL_F(path_link, NOP, "(ii*~i*~)i"),*/
	SYMBOL(path_open, "(ii*~iIIi*)i"),
	/*SYMBOL_F(fd_readdir, NOP, "(i*~I*)i"),
	SYMBOL_F(path_readlink, NOP, "(i*~*~*)i"),
	SYMBOL_F(path_rename, NOP, "(i*~i*~)i"),
	SYMBOL_F(fd_filestat_get, NOP, "(i*)i"),
	SYMBOL_F(fd_filestat_set_times, NOP, "(iIIi)i"),
	SYMBOL_F(fd_filestat_set_size, NOP, "(iI)i"),
	SYMBOL_F(path_filestat_get, NOP, "(ii*~*)i"),
	SYMBOL_F(path_filestat_set_times, NOP, "(ii*~IIi)i"),*/
	//SYMBOL(path_symlink, "(*~i*~)i"),
	//SYMBOL(path_unlink_file, "(i*~)i"),
	//SYMBOL(path_remove_directory, "(i*~)i"),
	//SYMBOL(poll_oneoff, "(**i*)i"),
	//SYMBOL(proc_exit, "(i)"),
	//SYMBOL(proc_raise, "(i)i"),
	//SYMBOL(random_get, "(*~)i"),
	//SYMBOL(sock_recv, "(i*ii**)i"),
	//SYMBOL(sock_send, "(i*ii*)i"),
	//SYMBOL(sock_shutdown, "(ii)i"),
	//SYMBOL(sched_yield, "()i"),
};

int32_t RegisterWasiNatives()
{
	fds.push_back({ .preopen = true });
	fds.push_back({ .preopen = true });
	fds.push_back({ .preopen = true });
	fds.push_back({ .preopen = true, .type = FILETYPE_DIRECTORY, .path = mod_directory });
	
	return wasm_runtime_register_natives("wasi_snapshot_preview1", native_symbols_libc_wasi, sizeof(native_symbols_libc_wasi) / sizeof(*native_symbols_libc_wasi));
}