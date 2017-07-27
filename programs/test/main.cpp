#include <appbase/application.hpp>
#include <steemit/manifest/plugins.hpp>

#include <steemit/protocol/types.hpp>
#include <steemit/protocol/version.hpp>

#include <fc/exception/exception.hpp>
#include <fc/thread/thread.hpp>
#include <fc/interprocess/signals.hpp>
#include <fc/log/console_appender.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/log/logger.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/json.hpp>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/program_options.hpp>

#include <iostream>
#include <csignal>
#include <vector>

namespace bpo = boost::program_options;
using steemit::protocol::version;
using std::string;
using std::vector;

fc::optional<fc::logging_config> load_logging_config( const bpo::variables_map& );

int main( int argc, char** argv )
{
   try
   {
      #ifdef IS_TEST_NET
      std::cerr << "------------------------------------------------------\n\n";
      std::cerr << "            STARTING TEST NETWORK\n\n";
      std::cerr << "------------------------------------------------------\n";
      auto initminer_private_key = graphene::utilities::key_to_wif( STEEMIT_INIT_PRIVATE_KEY );
      std::cerr << "initminer public key: " << STEEMIT_INIT_PUBLIC_KEY_STR << "\n";
      std::cerr << "initminer private key: " << initminer_private_key << "\n";
      std::cerr << "chain id: " << std::string( STEEMIT_CHAIN_ID ) << "\n";
      std::cerr << "blockchain version: " << fc::string( STEEMIT_BLOCKCHAIN_VERSION ) << "\n";
      std::cerr << "------------------------------------------------------\n";
#else
      std::cerr << "------------------------------------------------------\n\n";
      std::cerr << "            STARTING STEEM NETWORK\n\n";
      std::cerr << "------------------------------------------------------\n";
      std::cerr << "initminer public key: " << STEEMIT_INIT_PUBLIC_KEY_STR << "\n";
      std::cerr << "chain id: " << std::string( STEEMIT_CHAIN_ID ) << "\n";
      std::cerr << "blockchain version: " << fc::string( STEEMIT_BLOCKCHAIN_VERSION ) << "\n";
      std::cerr << "------------------------------------------------------\n";
#endif

      // Setup logging config
      bpo::options_description options;

      std::vector< std::string > default_console_appender( {"stderr","std_err"} );
      std::string str_default_console_appender = boost::algorithm::join( default_console_appender, " " );

      std::vector< std::string > default_file_appender( {"p2p","logs/p2p/p2p.log"} );
      std::string str_default_file_appender = boost::algorithm::join( default_file_appender, " " );

      std::vector< std::string > default_logger( {"default","warn","stderr","p2p","warn","p2p"} );
      std::string str_default_logger = boost::algorithm::join( default_logger, " " );

      options.add_options()
         ("log-console-appender", bpo::value< std::vector< std::string > >()->composing()->default_value( default_console_appender, str_default_console_appender ),
            "Console appender definitions: as name stream" )
         ("log-file-appender", bpo::value< std::vector< std::string > >()->composing()->default_value( default_file_appender, str_default_file_appender ),
            "File appender definitions: as name file" )
         ("log-logger", bpo::value< std::vector< std::string > >()->composing()->default_value( default_logger, str_default_logger ),
            "Logger definition as: name level appender" )
         ;

      appbase::app().add_program_options( bpo::options_description(), options );

      steemit::plugins::register_plugins();

      if( !appbase::app().initialize( argc, argv ) )
         return -1;

      try
      {
         fc::optional< fc::logging_config > logging_config = load_logging_config( appbase::app().get_args() );
         if( logging_config )
            fc::configure_logging( *logging_config );
      }
      catch( const fc::exception& )
      {
         wlog( "Error parsing logging config" );
      }

      appbase::app().startup();
      appbase::app().exec();
      std::cout << "exited cleanly\n";
      return 0;
   }
   catch ( const boost::exception& e )
   {
      std::cerr << boost::diagnostic_information(e) << "\n";
   }
   catch ( const std::exception& e )
   {
      std::cerr << e.what() << "\n";
   }
   catch ( ... )
   {
      std::cerr << "unknown exception\n";
   }

   return -1;
}

vector< string > tokenize_config_args( const vector< string >& args )
{
   vector< string > result;

   for( auto& a : args )
   {
      vector< string > tokens;
      boost::split( tokens, a, boost::is_any_of( " \t" ) );
      for( const auto& t : tokens )
         if( t.size() )
            result.push_back( t );
   }

   return result;
}

struct console_appender_arg
{
   std::string appender;
   std::string stream;
};

struct file_appender_arg
{
   std::string appender;
   std::string file;
};

struct logger_arg
{
   std::string name;
   std::string level;
   std::string appender;
};

fc::optional<fc::logging_config> load_logging_config( const bpo::variables_map& args )
{
   try
   {
      fc::logging_config logging_config;
      bool found_logging_config = false;

      if( args.count( "log-console-appender" ) )
      {
         std::vector< string > console_appender_args = args["log-console-appender"].as< vector< string > >();

         for( string& s : console_appender_args )
         {
            try
            {
               auto console_appender = fc::json::from_string( s ).as< console_appender_arg >();

               fc::console_appender::config console_appender_config;
               console_appender_config.level_colors.emplace_back(
                  fc::console_appender::level_color( fc::log_level::debug,
                                                   fc::console_appender::color::green));
               console_appender_config.level_colors.emplace_back(
                  fc::console_appender::level_color( fc::log_level::warn,
                                                   fc::console_appender::color::brown));
               console_appender_config.level_colors.emplace_back(
                  fc::console_appender::level_color( fc::log_level::error,
                                                   fc::console_appender::color::red));
               console_appender_config.stream = fc::variant( console_appender.stream ).as< fc::console_appender::stream::type >();
               logging_config.appenders.push_back(
                  fc::appender_config( console_appender.appender, "console", fc::variant( console_appender_config ) ) );
               found_logging_config = true;
            }
            catch( ... ) {}
         }
      }

      if( args.count( "log-file-appender" ) )
      {
         std::vector< string > file_appender_args = args["log-file-appender"].as< vector< string > >();

         for( string& s : file_appender_args )
         {
            auto file_appender = fc::json::from_string( s ).as< file_appender_arg >();

            fc::path file_name = file_appender.file;
            if( file_name.is_relative() )
               file_name = fc::absolute( appbase::app().data_dir() ) / file_name;

            // construct a default file appender config here
            // filename will be taken from ini file, everything else hard-coded here
            fc::file_appender::config file_appender_config;
            file_appender_config.filename = file_name;
            file_appender_config.flush = true;
            file_appender_config.rotate = true;
            file_appender_config.rotation_interval = fc::hours(1);
            file_appender_config.rotation_limit = fc::days(1);
            logging_config.appenders.push_back(
               fc::appender_config( file_appender.appender, "file", fc::variant( file_appender_config ) ) );
            found_logging_config = true;
         }
      }

      if( args.count( "log-logger" ) )
      {
         std::vector< string > logger_args = args[ "log-logger" ].as< std::vector< std::string > >();

         for( string& s : logger_args )
         {
            auto logger = fc::json::from_string( s ).as< logger_arg >();

            fc::logger_config logger_config( logger.name );
            logger_config.level = fc::variant( logger.level ).as< fc::log_level >();
            boost::split( logger_config.appenders, logger.appender,
                          boost::is_any_of(" ,"),
                          boost::token_compress_on );
            logging_config.loggers.push_back( logger_config );
            found_logging_config = true;
         }
      }

      if( found_logging_config )
         return logging_config;
      else
         return fc::optional< fc::logging_config >();
   }
   FC_RETHROW_EXCEPTIONS(warn, "")
}

FC_REFLECT( console_appender_arg, (appender)(stream) )
FC_REFLECT( file_appender_arg, (appender)(file) )
FC_REFLECT( logger_arg, (name)(level)(appender) )
