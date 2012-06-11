#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <libtorrent/torrent_handle.hpp>

#include "libed2k/log.hpp"
#include "libed2k/session.hpp"
#include "libed2k/session_settings.hpp"
#include "libed2k/util.hpp"
#include "libed2k/alert_types.hpp"
#include "libed2k/file.hpp"
#include "libed2k/search.hpp"
#include "libed2k/peer_connection_handle.hpp"

using namespace libed2k;

/**
 md4_hash::dump D4CF11BC699F0850210A92BEE8DFCD12
23:57.11 4860[dbg] net_identifier::dump(IP=1768108042 port=4662)
23:57.11 4861[dbg] size type is: 4
23:57.11 4862[dbg] count: 7
23:57.11 4863[dbg] base_tag::dump
23:57.11 4864[dbg] type: TAGTYPE_STRING
23:57.11 4865[dbg] name:  tag id: FT_FILENAME
23:57.11 4866[dbg] VALUEСоблазны в больнице.XXX
23:57.11 4867[dbg] base_tag::dump
23:57.11 4868[dbg] type: TAGTYPE_UINT32
23:57.11 4869[dbg] name:  tag id: FT_FILESIZE
23:57.11 4870[dbg] VALUE: 732807168
23:57.11 4871[dbg] base_tag::dump
23:57.11 4872[dbg] type: TAGTYPE_UINT8
23:57.11 4873[dbg] name:  tag id: FT_SOURCES
23:57.11 4874[dbg] VALUE: 43
23:57.11 4875[dbg] base_tag::dump
23:57.11 4876[dbg] type: TAGTYPE_UINT8
23:57.11 4877[dbg] name:  tag id: FT_COMPLETE_SOURCES
23:57.11 4878[dbg] VALUE: 42
23:57.11 4879[dbg] base_tag::dump
23:57.11 4880[dbg] type: TAGTYPE_UINT16
23:57.11 4881[dbg] name:  tag id: FT_MEDIA_BITRATE
23:57.11 4882[dbg] VALUE: 982
23:57.11 4883[dbg] base_tag::dump
23:57.11 4884[dbg] type: TAGTYPE_STR4
23:57.11 4885[dbg] name:  tag id: FT_MEDIA_CODEC
23:57.11 4886[dbg] VALUExvid
23:57.11 4887[dbg] base_tag::dump
23:57.11 4888[dbg] type: TAGTYPE_UINT16
23:57.11 4889[dbg] name:  tag id: FT_MEDIA_LENGTH
23:57.11 4890[dbg] VALUE: 5904
23:57.11 4891[dbg] search_file_entry::dump
23:57.11 4892[dbg] md4_hash::dump 6FE930EE2BB8B4E5B960811346331A43
23:57.11 4893[dbg] net_identifier::dump(IP=576732682 port=4666)
23:57.11 4894[dbg] size type is: 4
23:57.11 4895[dbg] count: 7
23:57.11 4896[dbg] base_tag::dump
23:57.11 4897[dbg] type: TAGTYPE_STRING
23:57.11 4898[dbg] name:  tag id: FT_FILENAME
23:57.11 4899[dbg] VALUEXXX Видео Bangbros. Nasty Naom.wmv
23:57.11 4900[dbg] base_tag::dump
23:57.11 4901[dbg] type: TAGTYPE_UINT32
23:57.11 4902[dbg] name:  tag id: FT_FILESIZE
23:57.11 4903[dbg] VALUE: 6779182
23:57.11 4904[dbg] base_tag::dump
23:57.11 4905[dbg] type: TAGTYPE_UINT8
23:57.11 4906[dbg] name:  tag id: FT_SOURCES
23:57.11 4907[dbg] VALUE: 2
23:57.11 4908[dbg] base_tag::dump
23:57.11 4909[dbg] type: TAGTYPE_UINT8
23:57.11 4910[dbg] name:  tag id: FT_COMPLETE_SOURCES
23:57.11 4911[dbg] VALUE: 2
23:57.11 4912[dbg] base_tag::dump
23:57.11 4913[dbg] type: TAGTYPE_UINT16
23:57.11 4914[dbg] name:  tag id: FT_MEDIA_BITRATE
23:57.11 4915[dbg] VALUE: 1017
23:57.11 4916[dbg] base_tag::dump
23:57.11 4917[dbg] type: TAGTYPE_STR4
23:57.11 4918[dbg] name:  tag id: FT_MEDIA_CODEC
23:57.11 4919[dbg] VALUEwmv2
23:57.11 4920[dbg] base_tag::dump
23:57.11 4921[dbg] type: TAGTYPE_UINT8
23:57.11 4922[dbg] name:  tag id: FT_MEDIA_LENGTH
23:57.11 4923[dbg] VALUE: 54
23:57.11 4924[dbg] Results count: 52


 */


int main(int argc, char* argv[])
{
    LOGGER_INIT()

    if (argc < 3)
    {
        ERR("Set server host and port");
        return (1);
    }

    DBG("Server: " << argv[1] << " port: " << argv[2]);

    libed2k::fingerprint print;
    libed2k::session_settings settings;
    settings.server_keep_alive_timeout = 300;
    settings.server_reconnect_timeout = 30;
    settings.server_hostname = argv[1];
    settings.server_timeout = 125;
    settings.server_port = atoi(argv[2]);
    //settings.server_
    libed2k::session ses(print, "0.0.0.0", settings);
    ses.set_alert_mask(alert::all_categories);

    /*
    sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_NOT));
    sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_AND));
    sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_AND));
    sr.add_entry(libed2k::search_request_entry("dead"));
    sr.add_entry(libed2k::search_request_entry("walking"));
    sr.add_entry(libed2k::search_request_entry(FT_FILESIZE, ED2K_SEARCH_OP_GREATER, 300));
    sr.add_entry(libed2k::search_request_entry("HD"));
*/
    //sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_NOT));
    //sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_OR));
    //sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_AND));
    //sr.add_entry(libed2k::search_request_entry("dead"));
    //sr.add_entry(libed2k::search_request_entry("kkkkJKJ"));
    libed2k::search_request order = libed2k::generateSearchRequest(0,0,0,0, "", "", "", 0, 0, "db2");

    std::cout << "---- libed2k_client started\n"
              << "---- press q to exit\n"
              << "---- press something other for process alerts " << std::endl;


    std::string strAlex = "109.191.73.222";
    std::string strDore = "192.168.161.54";
    std::string strDiman = "88.206.52.81";
    ip::address a(ip::address::from_string(strDore.c_str()));
    int nPort = 4665;

    DBG("addr: "<< int2ipstr(address2int(a)));
    std::string strUser;
    libed2k::peer_connection_handle pch;

    net_identifier ni(address2int(a), nPort);
    while ((std::cin >> strUser))
    {
        if (strUser == "quit")
        {
            break;
        }

        if (!strUser.empty() && strUser.size() == 1)
        {
            switch(strUser.at(0))
            {
            case 'l':
                ses.post_search_request(order);
                break;
            case 'd':
                ses.server_conn_stop();
                break;
            case 'c':
                ses.server_conn_start();
                break;
            case 'f':
                {
                    if (pch.empty())
                    {
                        pch = ses.add_peer_connection(ni);
                    }

                    DBG("get shared files");
                    pch.get_shared_files();
                    break;
                }
            case 'm':
                {
                    if (pch.empty())
                    {
                        DBG("pch empty - create it");
                        pch = ses.add_peer_connection(ni);
                    }

                    DBG("pch send message");
                    pch.send_message("Hello it is peer connection handle");
                }
                break;
            case 's':
            {
                if (pch.empty())
                {
                    pch = ses.add_peer_connection(ni);
                }

                pch.get_shared_files();
            }
                break;
            case 'r':
            {
                if (pch.empty())
                {
                    pch = ses.add_peer_connection(ni);
                }

                DBG("get shared directories");

                pch.get_shared_directories();
                break;
            }
            case 'e':
            {
                if (pch.empty())
                {
                    pch = ses.add_peer_connection(ni);
                }

                DBG("get shared files");
                pch.get_shared_directory_files("/home/d95a1/sqllib/samples/cpp");
                break;
            }
            case 'i':
            {
                pch = ses.find_peer_connection(ni);

                if (pch.empty())
                {
                    DBG("peer connection not exists - add it");
                    pch = ses.add_peer_connection(ni);
                }
                break;
            }
            default:
                break;
            };
        }

        if (strUser.size() > 1)
        {
            if (!pch.empty())
            {
                pch.send_message(strUser);
            }
        }


        std::auto_ptr<alert> a = ses.pop_alert();

        while(a.get())
        {
            if (dynamic_cast<server_connection_initialized_alert*>(a.get()))
            {
                server_connection_initialized_alert* p = dynamic_cast<server_connection_initialized_alert*>(a.get());
                std::cout << "server initalized: cid: "
                        << p->m_nClientId
                        << std::endl;
                DBG("send search request");
                ses.post_search_request(order);
            }
            else if (dynamic_cast<server_name_resolved_alert*>(a.get()))
            {
                DBG("server name was resolved: " << dynamic_cast<server_name_resolved_alert*>(a.get())->m_strServer);
            }
            else if (dynamic_cast<server_status_alert*>(a.get()))
            {
                server_status_alert* p = dynamic_cast<server_status_alert*>(a.get());
                DBG("server status: files count: " << p->m_nFilesCount << " users count " << p->m_nUsersCount);
            }
            else if (dynamic_cast<server_message_alert*>(a.get()))
            {
                server_message_alert* p = dynamic_cast<server_message_alert*>(a.get());
                std::cout << "msg: " << p->m_strMessage << std::endl;
            }
            else if (dynamic_cast<server_identity_alert*>(a.get()))
            {
                server_identity_alert* p = dynamic_cast<server_identity_alert*>(a.get());
                DBG("server_identity_alert: " << p->m_hServer << " name:  " << p->m_strName << " descr: " << p->m_strDescr);
            }
            else if (shared_files_alert* p = dynamic_cast<shared_files_alert*>(a.get()))
            {
                DBG("RESULT: " << p->m_files.m_collection.size());
                p->m_files.dump();

                if (shared_directory_files_alert* p2 = dynamic_cast<shared_directory_files_alert*>(p))
                {
                    DBG("shared dir files: " << int2ipstr(p2->m_np.m_nIP) << " count " << p2->m_files.m_collection.size() << " for " << p2->m_strDirectory);
                    //p->m_files.dump();
                }
                else
                {

                    // ok, prepare to get sources
                    //p->m_result.dump();
                    DBG("Results count: " << p->m_files.m_collection.size());

                    if (p->m_more)
                    {
                        DBG("Request more results");
                        ses.post_search_more_result_request();
                    }
                }

                /*
                for (int n = 0; n < p->m_list.m_collection.size(); n++)
                {

                }
*/
                //if (p->m_list.m_collection.size() > 10)
                //{
                    // generate continue request
                    //search_request sr2;
                    //ses.post_search_more_result_request();
                //}

                //int nIndex = p->m_result.m_results_list.m_collection.size() - 1;

                //if (nIndex > 0)
                //{

                    //DBG("Search related files");

                    //search_request sr2 = generateSearchRequest(p->m_list.m_collection[nIndex].m_hFile);
                    //ses.post_search_request(sr2);

                    /*

                    const boost::shared_ptr<base_tag> src = p->m_list.m_collection[nIndex].m_list.getTagByNameId(FT_COMPLETE_SOURCES);
                    const boost::shared_ptr<base_tag> sz = p->m_list.m_collection[nIndex].m_list.getTagByNameId(FT_FILESIZE);

                    if (src.get() && sz.get())
                    {
                        DBG("Complete sources: " << src.get()->asInt());
                        DBG("Size: " << sz.get()->asInt());
                        ses.post_sources_request(p->m_list.m_collection[nIndex].m_hFile, sz.get()->asInt());
                        // request sources

                    }
                    */

                //}
            }
            else if(dynamic_cast<peer_message_alert*>(a.get()))
            {
                peer_message_alert* p = dynamic_cast<peer_message_alert*>(a.get());
                DBG("MSG: ADDR: " << int2ipstr(p->m_np.m_nIP) << " MSG " << p->m_strMessage);
            }
            else if (peer_disconnected_alert* p = dynamic_cast<peer_disconnected_alert*>(a.get()))
            {
                DBG("peer disconnected: " << libed2k::int2ipstr(p->m_np.m_nIP));
            }
            else if (peer_captcha_request_alert* p = dynamic_cast<peer_captcha_request_alert*>(a.get()))
            {
                DBG("captcha request ");
                FILE* fp = fopen("./captcha.bmp", "wb");

                if (fp)
                {
                    fwrite(&p->m_captcha[0], 1, p->m_captcha.size(), fp);
                    fclose(fp);
                }

            }
            else if (peer_captcha_result_alert* p = dynamic_cast<peer_captcha_result_alert*>(a.get()))
            {
                DBG("captcha result " << p->m_nResult);
            }
            else if (peer_connected_alert* p = dynamic_cast<peer_connected_alert*>(a.get()))
            {
                DBG("peer connected: " << int2ipstr(p->m_np.m_nIP) << " status: " << p->m_active);
            }
            else if (shared_files_access_denied* p = dynamic_cast<shared_files_access_denied*>(a.get()))
            {
                DBG("peer denied access to shared files: " << int2ipstr(p->m_np.m_nIP));
            }
            else if (shared_directories_alert* p = dynamic_cast<shared_directories_alert*>(a.get()))
            {
                DBG("peer shared directories: " << int2ipstr(p->m_np.m_nIP) << " count: " << p->m_dirs.size());

                for (size_t n = 0; n < p->m_dirs.size(); ++n)
                {
                    DBG("DIR: " << p->m_dirs[n]);
                }
            }
            else
            {
                std::cout << "Unknown alert " << std::endl;
            }

            a = ses.pop_alert();
        }
    }

    return 0;
}


