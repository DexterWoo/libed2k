
#include "libed2k/session.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/transfer.hpp"
#include "libed2k/peer.hpp"
#include "libed2k/peer_connection.hpp"
#include "libed2k/server_connection.hpp"
#include "libed2k/constants.hpp"
#include "libed2k/util.hpp"
#include "libed2k/file.hpp"
#include "libed2k/alert_types.hpp"

namespace libed2k
{

    transfer::transfer(aux::session_impl& ses, ip::tcp::endpoint const& net_interface,
                   int seq, add_transfer_params const& p):
        m_ses(ses),
        m_obsolete(false),
        m_announced(false),
        m_abort(false),
        m_paused(false),
        m_sequential_download(false),
        m_sequence_number(seq),
        m_net_interface(net_interface.address(), 0),
        m_filehash(p.file_hash),
        m_hashset(p.piece_hash),
        m_filepath(convert_from_native(p.file_path.string())),
        m_filesize(p.file_size),
        m_storage_mode(p.storage_mode),
        m_state(transfer_status::checking_resume_data),
        m_seed_mode(p.seed_mode),
        m_policy(this, p.peer_list),
        m_info(new libtorrent::torrent_info(libtorrent::sha1_hash())),
        m_accepted(p.m_accepted),
        m_requested(p.m_requested),
        m_transferred(p.m_transferred),
        m_priority(p.m_priority)
    {
        if (m_hashset.pieces().size() == 0)
            m_hashset.reset(div_ceil(m_filesize, PIECE_SIZE));

        assert(m_hashset.pieces().size() == num_pieces());
    }

    transfer::~transfer()
    {
        if (!m_connections.empty())
            disconnect_all(errors::transfer_aborted);
    }

    transfer_handle transfer::handle()
    {
        return transfer_handle(shared_from_this());
    }

    void transfer::start()
    {
        if (!m_seed_mode)
        {
            m_picker.reset(new libtorrent::piece_picker());
            // TODO: resume data, file progress
        }

        init();
    }

    void transfer::abort()
    {
        if (m_abort) return;
        m_abort = true;

        DBG("abort transfer {hash: " << hash() << "}");

        // disconnect all peers and close all
        // files belonging to the torrents
        disconnect_all(errors::transfer_aborted);

        // post a message to the main thread to destruct
        // the torrent object from there
        if (m_owning_storage.get())
        {
            m_storage->abort_disk_io();
            m_storage->async_release_files(
                boost::bind(&transfer::on_transfer_aborted, shared_from_this(), _1, _2));
        }

        //dequeue_transfer_check();

        if (m_state == transfer_status::checking_files)
            set_state(transfer_status::queued_for_checking);

        m_owning_storage = 0;
    }

    void transfer::set_state(transfer_status::state_t s)
    {
        if (m_state == s) return;
        m_ses.m_alerts.post_alert_should(state_changed_alert(handle(), s, m_state));
        m_state = s;
    }

    void transfer::set_obsolete(bool obsolete)
    {
        if (is_finished())
        {
            m_obsolete = obsolete;
        }
    }

    bool transfer::want_more_peers() const
    {
        return !is_finished() && m_policy.num_peers() == 0;
    }

    void transfer::request_peers()
    {
        APP("request peers by hash: " << m_filehash << ", size: " << m_filesize);
        m_ses.m_server_connection->post_sources_request(m_filehash, m_filesize);
    }

    void transfer::add_peer(const tcp::endpoint& peer)
    {
        m_policy.add_peer(peer);
    }

    bool transfer::want_more_connections() const
    {
        return !m_abort && !is_paused() && !is_seed() && m_policy.num_connect_candidates() > 0;
    }

    bool transfer::connect_to_peer(peer* peerinfo)
    {
        tcp::endpoint ep(peerinfo->endpoint);
        boost::shared_ptr<tcp::socket> sock(new tcp::socket(m_ses.m_io_service));
        m_ses.setup_socket_buffers(*sock);

        boost::intrusive_ptr<peer_connection> c(
            new peer_connection(m_ses, shared_from_this(), sock, ep, peerinfo));

        // add the newly connected peer to this transfer's peer list
        m_connections.insert(boost::get_pointer(c));
        m_ses.m_connections.insert(c);
        m_policy.set_connection(peerinfo, c.get());
        c->start();

        int timeout = m_ses.settings().peer_connect_timeout;

        try
        {
            m_ses.m_half_open.enqueue(
                boost::bind(&peer_connection::connect, c, _1),
                boost::bind(&peer_connection::on_timeout, c),
                libtorrent::seconds(timeout));
        }
        catch (std::exception&)
        {
            std::set<peer_connection*>::iterator i =
                m_connections.find(boost::get_pointer(c));
            if (i != m_connections.end()) m_connections.erase(i);
            c->disconnect(errors::no_error, 1);
            return false;
        }


        return peerinfo->connection;
    }

    bool transfer::attach_peer(peer_connection* p)
    {
        // TODO: check transfer for validness

        if (m_ses.m_connections.find(p) == m_ses.m_connections.end())
        {
            return false;
        }
        if (m_ses.is_aborted())
        {
            return false;
        }
        if (!m_policy.new_connection(*p))
            return false;

        m_connections.insert(p);
        return true;
    }

    void transfer::remove_peer(peer_connection* c)
    {
        // TODO: implement
        DBG("transfer::remove_peer(" << c << ")");

        std::set<peer_connection*>::iterator i = m_connections.find(c);
        if (i == m_connections.end())
        {
            assert(false);
            return;
        }

        if (ready_for_connections())
        {
            assert(c->get_transfer().lock().get() == this);

            if (c->is_seed())
            {
                if (m_picker.get())
                {
                    m_picker->dec_refcount_all();
                }
            }
            else
            {
                if (m_picker.get())
                {
                    const bitfield& pieces = c->remote_hashset().pieces();
                    if (pieces.size() > 0)
                        m_picker->dec_refcount(pieces);
                }
            }
        }

        m_policy.connection_closed(*c);
        c->set_peer(0);
        m_connections.erase(c);
    }

    void transfer::disconnect_all(const error_code& ec)
    {
        while (!m_connections.empty())
        {
            peer_connection* p = *m_connections.begin();
            DBG("*** CLOSING CONNECTION: " << ec.message());

            if (p->is_disconnecting())
                m_connections.erase(m_connections.begin());
            else
                p->disconnect(ec);
        }
    }

    bool transfer::try_connect_peer()
    {
        return m_policy.connect_one_peer();
    }

    void transfer::piece_passed(int index, const md4_hash& hash)
    {
        bool was_finished = (num_have() == num_pieces());
        we_have(index, hash);
        if (!was_finished && is_finished())
        {
            // transfer finished
            // i.e. all the pieces we're interested in have
            // been downloaded. Release the files (they will open
            // in read only mode if needed)
            finished();
            // if we just became a seed, picker is now invalid, since it
            // is deallocated by the torrent once it starts seeding
        }
    }

    void transfer::we_have(int index, const md4_hash& hash)
    {
        //TODO: update progress
        m_picker->we_have(index);
        m_hashset.hash(index, hash);
    }

    size_t transfer::num_pieces() const
    {
        return div_ceil(m_filesize, PIECE_SIZE);
    }

    // called when torrent is complete (all pieces downloaded)
    void transfer::completed()
    {
        m_picker.reset();

        set_state(transfer_status::seeding);
        //if (!m_announcing) return;

        //announce_with_tracker();
    }

    // called when torrent is finished (all interesting
    // pieces have been downloaded)
    void transfer::finished()
    {
        DBG("file transfer '" << m_filepath.filename() << "' completed");
        //TODO: post alert

        set_state(transfer_status::finished);
        //set_queue_position(-1);

        // we have to call completed() before we start
        // disconnecting peers, since there's an assert
        // to make sure we're cleared the piece picker
        if (is_seed()) completed();

        // disconnect all seeds
        std::vector<peer_connection*> seeds;
        for (std::set<peer_connection*>::iterator i = m_connections.begin();
             i != m_connections.end(); ++i)
        {
            peer_connection* p = *i;
            if (p->remote_hashset().pieces().count() == int(num_have()))
                seeds.push_back(p);
        }
        std::for_each(seeds.begin(), seeds.end(),
                      boost::bind(&peer_connection::disconnect, _1, errors::transfer_finished, 0));

        if (m_abort) return;

        // we need to keep the object alive during this operation
        m_storage->async_release_files(
            boost::bind(&transfer::on_files_released, shared_from_this(), _1, _2));
    }

    void transfer::pause()
    {
        if (m_paused) return;
        m_paused = true;
        if (m_ses.is_paused()) return;

        DBG("pause transfer {hash: " << hash() << "}");

        // this will make the storage close all
        // files and flush all cached data
        if (m_owning_storage.get())
        {
            assert(m_storage);
            m_storage->async_release_files(
                boost::bind(&transfer::on_transfer_paused, shared_from_this(), _1, _2));
            m_storage->async_clear_read_cache();
        }
        else
        {
            m_ses.m_alerts.post_alert_should(paused_transfer_alert(handle()));
        }

        disconnect_all(errors::transfer_paused);
    }

    void transfer::resume()
    {
        if (!m_paused) return;
        DBG("resume transfer {hash: " << hash() << "}");
        m_paused = false;
        m_ses.m_alerts.post_alert_should(resumed_transfer_alert(handle()));
    }

    void transfer::set_upload_limit(int limit)
    {

    }

    int transfer::upload_limit() const
    {
        return 0;
    }

    void transfer::set_download_limit(int limit)
    {

    }

    int transfer::download_limit() const
    {
        return 0;
    }

    void transfer::delete_files()
    {
        DBG("deleting files in transfer");

        disconnect_all(errors::transfer_removed);

        if (m_owning_storage.get())
        {
            assert(m_storage);
            m_storage->async_delete_files(
                boost::bind(&transfer::on_files_deleted, shared_from_this(), _1, _2));
        }
    }

    void transfer::set_sequential_download(bool sd)
    {
        m_sequential_download = sd;
    }


    void transfer::piece_failed(int index)
    {
    }

    void transfer::restore_piece_state(int index)
    {
    }

    bool transfer::is_paused() const
    {
        return m_paused || m_ses.is_paused();
    }

    transfer_status transfer::status() const
    {
        transfer_status st;

        st.seed_mode = m_seed_mode;
        st.paused = m_paused;

        bytes_done(st);

        st.num_peers = (int)std::count_if(
            m_connections.begin(), m_connections.end(),
            !boost::bind(&peer_connection::is_connecting, _1));

        st.list_peers = m_policy.num_peers();
        //st.list_seeds = m_policy.num_seeds();
        st.connect_candidates = m_policy.num_connect_candidates();
        st.num_connections = m_connections.size();
        //st.connections_limit = m_max_connections;

        st.state = m_state;

        return st;
    }

    // fills in total_wanted, total_wanted_done and total_done
    void transfer::bytes_done(transfer_status& st) const
    {
        st.total_wanted = filesize();
        st.total_done = std::min<size_t>(num_have() * PIECE_SIZE, st.total_wanted);
        st.total_wanted_done = st.total_done;

        if (!m_picker) return;

        const int last_piece = num_pieces() - 1;

        // if we have the last piece, we have to correct
        // the amount we have, since the first calculation
        // assumed all pieces were of equal size
        if (m_picker->have_piece(last_piece))
        {
            int corr = filesize() % PIECE_SIZE - PIECE_SIZE;
            assert(corr <= 0);
            assert(corr > -int(PIECE_SIZE));
            st.total_done += corr;
            if (m_picker->piece_priority(last_piece) != 0)
            {
                assert(st.total_wanted_done >= int(PIECE_SIZE));
                st.total_wanted_done += corr;
            }
        }
        assert(st.total_wanted >= st.total_wanted_done);

        const std::vector<piece_picker::downloading_piece>& dl_queue =
            m_picker->get_download_queue();

        const int blocks_per_piece = div_ceil(PIECE_SIZE, BLOCK_SIZE);

        // look at all unfinished pieces and add the completed
        // blocks to our 'done' counter
        for (std::vector<piece_picker::downloading_piece>::const_iterator i =
                 dl_queue.begin(); i != dl_queue.end(); ++i)
        {
            int corr = 0;
            int index = i->index;
            // completed pieces are already accounted for
            if (m_picker->have_piece(index)) continue;
            assert(i->finished <= m_picker->blocks_in_piece(index));

            for (int j = 0; j < blocks_per_piece; ++j)
            {
                if (i->info[j].state == piece_picker::block_info::state_writing ||
                    i->info[j].state == piece_picker::block_info::state_finished)
                {
                    corr += block_bytes_wanted(piece_block(index, j));
                }
                assert(corr >= 0);
                assert(index != last_piece || j < m_picker->blocks_in_last_piece() ||
                       i->info[j].state != piece_picker::block_info::state_finished);
            }

            st.total_done += corr;
            if (m_picker->piece_priority(index) > 0)
                st.total_wanted_done += corr;
        }

        std::map<piece_block, int> downloading_piece;
        for (std::set<peer_connection*>::const_iterator i = m_connections.begin();
             i != m_connections.end(); ++i)
        {
            peer_connection* pc = *i;
            boost::optional<piece_block_progress> p = pc->downloading_piece_progress();
            if (!p) continue;

            if (m_picker->have_piece(p->piece_index))
                continue;

            piece_block block(p->piece_index, p->block_index);
            if (m_picker->is_finished(block))
                continue;

            std::map<piece_block, int>::iterator dp = downloading_piece.find(block);
            if (dp != downloading_piece.end())
            {
                if (dp->second < p->bytes_downloaded)
                    dp->second = p->bytes_downloaded;
            }
            else
            {
                downloading_piece[block] = p->bytes_downloaded;
            }
        }

        for (std::map<piece_block, int>::iterator i = downloading_piece.begin();
             i != downloading_piece.end(); ++i)
        {
            int done = std::min(block_bytes_wanted(i->first), i->second);
            st.total_done += done;
            if (m_picker->piece_priority(i->first.piece_index) != 0)
                st.total_wanted_done += done;
        }
    }

    void transfer::on_files_released(int ret, disk_io_job const& j)
    {
        // do nothing
    }

    void transfer::on_files_deleted(int ret, disk_io_job const& j)
    {
        boost::mutex::scoped_lock l(m_ses.m_mutex);

        if (ret != 0)
            m_ses.m_alerts.post_alert_should(delete_failed_transfer_alert(handle(), j.error));
        else
            m_ses.m_alerts.post_alert_should(deleted_transfer_alert(handle(), hash()));
    }

    void transfer::on_transfer_aborted(int ret, disk_io_job const& j)
    {
        // the transfer should be completely shut down now, and the
        // destructor has to be called from the main thread
    }

    void transfer::on_transfer_paused(int ret, disk_io_job const& j)
    {
        boost::mutex::scoped_lock l(m_ses.m_mutex);

        m_ses.m_alerts.post_alert_should(paused_transfer_alert(handle()));
    }

    void transfer::on_disk_error(disk_io_job const& j, peer_connection* c)
    {
        if (!j.error) return;
        ERR("disk error: '" << j.error.message() << " in file " << j.error_file);
    }

    void transfer::init()
    {
        file_storage& files = const_cast<file_storage&>(m_info->files());
        files.set_num_pieces(num_pieces());
        files.set_piece_length(PIECE_SIZE);
        files.add_file(m_filepath.filename(), m_filesize);
        //files.set_name(name);

        // the shared_from_this() will create an intentional
        // cycle of ownership, see the hpp file for description.
        m_owning_storage = new piece_manager(
            shared_from_this(), m_info, m_filepath.parent_path(), m_ses.m_filepool,
            m_ses.m_disk_thread, libtorrent::default_storage_constructor,
            static_cast<libtorrent::storage_mode_t>(m_storage_mode));
        m_storage = m_owning_storage.get();

        if (has_picker())
        {
            int blocks_per_piece = div_ceil(PIECE_SIZE, BLOCK_SIZE);
            int blocks_in_last_piece = div_ceil(m_filesize % PIECE_SIZE, BLOCK_SIZE);
            m_picker->init(blocks_per_piece, blocks_in_last_piece, num_pieces());
        }

        // TODO: checking resume data

        if (!is_seed()) set_state(transfer_status::downloading);
    }

    void transfer::second_tick()
    {
        if (want_more_peers()) request_peers();

        for (std::set<peer_connection*>::iterator i = m_connections.begin();
             i != m_connections.end(); ++i)
        {
            peer_connection* p = *i;

            try
            {
                p->second_tick();
            }
            catch (std::exception& e)
            {
                DBG("**ERROR**: " << e.what());
                p->disconnect(errors::no_error, 1);
            }
        }
    }

    void transfer::async_verify_piece(
        int piece_index, const md4_hash& hash, const boost::function<void(int)>& fun)
    {
        //TODO: piece verification
        m_ses.m_io_service.post(boost::bind(fun, 0));
    }

    // passed_hash_check
    //  0: success, piece passed check
    // -1: disk failure
    // -2: piece failed check
    void transfer::piece_finished(int index, const md4_hash& hash, int passed_hash_check)
    {
        // even though the piece passed the hash-check
        // it might still have failed being written to disk
        // if so, piece_picker::write_failed() has been
        // called, and the piece is no longer finished.
        // in this case, we have to ignore the fact that
        // it passed the check
        if (!m_picker->is_piece_finished(index)) return;

        if (passed_hash_check == 0)
        {
            // the following call may cause picker to become invalid
            // in case we just became a seed
            piece_passed(index, hash);
        }
        else if (passed_hash_check == -2)
        {
            // piece_failed() will restore the piece
            piece_failed(index);
        }
        else
        {
            m_picker->restore_piece(index);
            restore_piece_state(index);
        }
    }

    void transfer::announce()
    {
        // announce always now
        shared_file_entry entry = getAnnounce();
        m_ses.announce(entry);
    }

    shared_file_entry transfer::getAnnounce() const
    {
        shared_file_entry entry;
        // TODO - implement generate file entry from transfer here
        entry.m_hFile = m_filehash;
        entry.m_network_point.m_nIP     = m_ses.m_client_id;
        entry.m_network_point.m_nPort   = m_ses.settings().listen_port;
        entry.m_list.add_tag(make_string_tag(m_filepath.filename(), FT_FILENAME, true));

        __file_size fs;
        fs.nQuadPart = m_filesize;
        entry.m_list.add_tag(make_typed_tag(fs.nLowPart, FT_FILESIZE, true));

        if (fs.nHighPart > 0)
        {
            entry.m_list.add_tag(make_typed_tag(fs.nHighPart, FT_FILESIZE_HI, true));
        }

        bool bFileTypeAdded = false;

        if (m_ses.m_tcp_flags & SRV_TCPFLG_TYPETAGINTEGER)
        {
            // Send integer file type tags to newer servers
            boost::uint32_t eFileType = GetED2KFileTypeSearchID(GetED2KFileTypeID(m_filepath.string()));

            if (eFileType >= ED2KFT_AUDIO && eFileType <= ED2KFT_EMULECOLLECTION)
            {
                entry.m_list.add_tag(make_typed_tag(eFileType, FT_FILETYPE, true));
                bFileTypeAdded = true;
            }
        }

        if (!bFileTypeAdded)
        {
            // Send string file type tags to:
            //  - newer servers, in case there is no integer type available for the file type (e.g. emulecollection)
            //  - older servers
            //  - all clients
            std::string strED2KFileType(GetED2KFileTypeSearchTerm(GetED2KFileTypeID(m_filepath.string())));

            if (!strED2KFileType.empty())
            {
                entry.m_list.add_tag(make_string_tag(strED2KFileType, FT_FILETYPE, true));
            }
        }

        return entry;
    }

}