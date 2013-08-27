/*
	Copyright (C) 2013 by Pierre Talbot <ptalbot@mopong.net>
	Part of the Battle for Wesnoth Project http://www.wesnoth.org/

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY.

	See the COPYING file for more details.
*/


#include "umcd/server_options.hpp"
#include "umcd/server/multi_threaded/server_mt.hpp"
#include "umcd/server/daemon.hpp"
#include "umcd/protocol/entry_point.hpp"
#include "umcd/logger/asio_logger.hpp"
#include "umcd/otl/otl.hpp"
#include "umcd/env/environment_loader.hpp"
#include "umcd/env/database_info.hpp"
#include "umcd/env/protocol_info.hpp"

#include "wml_exception.hpp"
#include "config.hpp"

#include "umcd/boost/thread/workaround.hpp"
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/format.hpp>

#include <stdexcept>

using namespace umcd;

int main(int argc, char *argv[])
{
	try
	{
		server_options options(argc, argv);
		if(!options.is_info())
		{
			config cfg = options.read_config();

			environment_loader env_loader;
			env_loader.init_once();
			env_loader.load(cfg);

			// Badly the environment must me loaded before we can validate.
			// (Because the game_config::path must be initialized to validate).
			options.validate(cfg);

			asio_logger::get().load(logging_info());

			if(options.is_daemon())
			{
				boost::optional<std::string> err = launch_daemon();
				if(err)
				{
					UMCD_LOG(error) << *err;
					UMCD_LOG(warning) << "The server has been launched in frontend mode.";
				}
			}

			otl_connect db;
			try
			{
				database_info db_info;
				db.rlogon(boost::str(boost::format("UID=%1%;PWD=%2%;DSN=%3%") % db_info.user() % db_info.password() % db_info.dsn()).c_str());
			}
			catch(otl_exception& e)
			{
				std::cerr<<e.msg<<std::endl; // print out error message
				std::cerr<<e.stm_text<<std::endl; // print out SQL that caused the error
				std::cerr<<e.sqlstate<<std::endl; // print out SQLSTATE message
				std::cerr<<e.var_info<<std::endl; // print out the variable that caused the error
			}

			server_mt addon_server(
				server_core(),
				protocol_entry_point
			);

			// Launch logger.
			asio_logger logger(boost::ref(addon_server.io_service()), boost::posix_time::milliseconds(500));

			UMCD_LOG(info) << addon_server.thread_pool_size() << " cores found.";

			addon_server.run();
		}
	}
	catch(const twml_exception& e)
	{
		UMCD_LOG(fatal) << " (user message=" << e.user_message << " ; dev message=" << e.dev_message << ")";
	}
	catch(std::exception &e)
	{
		UMCD_LOG(fatal) << e.what();
	}
	RUN_ONCE_LOGGER();
	return 0;
}
