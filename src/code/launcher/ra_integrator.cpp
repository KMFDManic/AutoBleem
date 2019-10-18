//
// Created by screemer on 2019-07-25.
//

#include "ra_integrator.h"
#include "fstream"
#include <cstdio>
#include <iostream>
#include <sstream>
#include <algorithm>
#include "../util.h"
#include "../nlohmann/json.h"
#include "../engine/cfgprocessor.h"
#include "../DirEntry.h"
#include "../environment.h"

using namespace nlohmann;

//********************
// RAIntegrator::~RAIntegrator()
//********************
RAIntegrator::~RAIntegrator() {
    for (CoreInfoPtr ci: cores) {
        ci.reset();
    }
}

//********************
// RAIntegrator::isValidPlaylist
//********************
bool RAIntegrator::isValidPlaylist(string path) {
    // check file extension
    if (ReturnLowerCase(DirEntry::getFileExtension(path)) != "lpl") {
        cout << "Extension is not .lpl" << endl;
        return false;
    }
    // check if not empty
    std::ifstream in(path, std::ifstream::ate | std::ifstream::binary);
    if (in.tellg() <= 0) {
        cout << "Playlist looks like empty file" << endl;
        return false;
    }

    return true;
}

//********************
// RAIntegrator::findOverrideCore
//********************
bool RAIntegrator::findOverrideCore(PsGamePtr game, string &core_name, string &core_path) {
    string dbName = DirEntry::getFileNameWithoutExtension(game->db_name);

    lcase(dbName);    trim(dbName);
    map<string, CoreInfoPtr>::const_iterator pos = overrideCores.find(dbName);
    if (pos == overrideCores.end()) {
        core_name = "DETECT";
        core_path = "DETECT";
        return false;
    }
    core_name = pos->second->name;
    core_path = pos->second->core_path;
    return true;
}

//********************
// RAIntegrator::isJSONPlaylist
//********************
bool RAIntegrator::isJSONPlaylist(string path) {
    std::ifstream in(path, std::ifstream::binary);
    string line;
    getline(in, line);
    trim(line);
    if (line.empty()) {
        return false;
    }
    if (line == "{")
        return true;
    return false;
}

//********************
// RAIntegrator::parseJSON
//********************
PsGames RAIntegrator::parseJSON(string path) {
    std::ifstream in(path, std::ifstream::binary);
    json j;
    in >> j;

    int id = 0;
    PsGames psGames;

    json array = j["items"];

    for (const auto & item : array) {
        PsGamePtr game{new PsGame};
        game->gameId = id++;
        game->title = item["label"];
        game->publisher = "";
        game->year = 0;
        game->players = 0;
        game->folder = "";
        game->ssFolder = "";
        game->base = "";
        game->memcard = "";
        game->cds = 0;
        game->locked = true;
        game->hd = false;
        game->favorite = false;
        game->foreign = true;
        game->core_name = item["core_name"];
        game->core_path = item["core_path"];
        game->db_name = item["db_name"];
        game->image_path = item["path"];

#if defined(__x86_64__) || defined(_M_X64)
        // if you are running in the debugging environment then /media might be shared drive /media/sf_G_DRIVE etc
        if (game->image_path.substr(0, 6) == "/media")
            game->image_path.replace(0, 6, Env::getPathToUSBRoot());
        if (game->core_path.substr(0, 6) == "/media")
            game->core_path.replace(0, 6, Env::getPathToUSBRoot());
#endif

        if ((game->core_path == "DETECT") || (game->core_name == "DETECT")) {
            autoDetectCorePath(game, game->core_name, game->core_path);
        }

        if (!DirEntry::exists(game->core_path)) {
            autoDetectCorePath(game, game->core_name, game->core_path);
        }
        if (isGameValid(game)) {
            psGames.emplace_back(game);
        } else {
            cout << "Game  invalid: " << game->title <<  endl;
        }
    }
    in.close();
    return psGames;
}

//********************
// RAIntegrator::parse6line
// this routine is called when the playlist file is NOT a json file
//********************
PsGames RAIntegrator::parse6line(string path) {
    PsGames psGames;
    std::ifstream in(path, ifstream::binary);

    string game_path = "";
    string label = "";
    string core_path = "";
    string core_name = "";
    string crc = "";
    string db_name = "";

    int id = 0;
    while (!in.eof()) {

        getline(in, game_path);
        if (in.eof()) break;
        getline(in, label);
        if (in.eof()) break;
        getline(in, core_path);
        if (in.eof()) break;
        getline(in, core_name);
        if (in.eof()) break;
        getline(in, crc);
        if (in.eof()) break;
        getline(in, db_name);

        PsGamePtr game{new PsGame};
        game->gameId = id++;
        game->title = label;
        game->publisher = "";
        game->year = 0;
        game->players = 0;
        game->folder = "";
        game->ssFolder = "";
        game->base = "";
        game->memcard = "";
        game->cds = 0;
        game->locked = true;
        game->hd = false;
        game->favorite = false;
        game->foreign = true;
        game->core_name = core_name;
        game->core_path = core_path;
        game->db_name = db_name;
        game->image_path = game_path;
        if ((core_path == "DETECT") || (core_name == "DETECT")) {
            bool coreFound = autoDetectCorePath(game, core_name, core_path);
            if (!coreFound) continue;
            game->core_name = core_name;
            game->core_path = core_path;
        }
        if (!DirEntry::exists(game->core_path)) {
            bool coreFound = autoDetectCorePath(game, core_name, core_path);
            if (!coreFound) continue;
            game->core_name = core_name;
            game->core_path = core_path;

        }
        if (isGameValid(game)) {
            psGames.emplace_back(game);
        }
    }
    in.close();
    return psGames;
}

//********************
// RAIntegrator::playlistNameToIndex
//********************
// returns true for success, and the index if successful
std::tuple<bool,int> RAIntegrator::playlistNameToIndex(const string& name) {
    auto it = find_if(begin(playlistInfos), end(playlistInfos),
            [&] (RAPlaylistInfo& info) { return name == info.name; } );
    if (it != end(playlistInfos)) {
        return make_tuple(true, it - begin(playlistInfos));
    } else
        return make_tuple(false, -1);
}

//********************
// RAIntegrator::readInAllData
//********************
// reads all the playlist info into RAPlaylistInfos
void RAIntegrator::readInAllData() {
    assert(playlistInfos.size() == 0);
    if (playlistInfos.size() != 0) {
        cout << "called readInAllData when the data was already read in" << endl;
        return;
    }

    string path = Env::getPathToRetroarchDir() + sep + "playlists";
    cout << "Checking playlists path" << path << endl;

    if (DirEntry::exists(path)) {
        vector<DirEntry> entries = DirEntry::diru_FilesOnly(path);
        cout << "Total Playlists:" << entries.size() << endl;
        vector<string> playlistNames;
        string favoritesName;
        for (const DirEntry &entry:entries) {
            if (DirEntry::getFileNameWithoutExtension(entry.name) == "AutoBleem")
                continue;

            if (Util::compareCaseInsensitive("favorites.lpl", entry.name))
                favoritesName = entry.name; // save for the end
            else
                playlistNames.emplace_back(entry.name);
        }

        // sort the playlist names and add favorites at the end if any
        sort(begin(playlistNames), end(playlistNames));
        if (favoritesName != "")
            playlistNames.emplace_back(favoritesName);

        for (auto & playlistName : playlistNames) {
            cout << "Parsing Playlist:" << playlistName << endl;
            string path = Env::getPathToRetroarchDir() + sep + "playlists" + sep + playlistName;
            if (!isValidPlaylist(path))
                continue;
            PsGames games;
            if (isJSONPlaylist(path)) {
                games = parseJSON(path);
            } else {
                games = parse6line(path);
            }
            cout << "Games found:" << games.size() << endl;
            if (games.size() > 0)
                playlistInfos.emplace_back(playlistName, games);
        }
    }

//            if (isValidPlaylist(path + sep + entry.name)) {
//                if (getGamesNumber(entry.name) > 0) {
}

#if 0
//********************
// RAIntegrator::getGames
//********************
bool RAIntegrator::getGames(PsGames *result, string playlist) {
    if (playlist != "") {
        cout << "Parsing Playlist:" << playlist << endl;
        string path = Env::getPathToRetroarchDir() + sep + "playlists" + sep + playlist;
        if (isJSONPlaylist(path)) {
            parseJSON(result, path);
        } else {
            parse6line(result, path);
        }
        cout << "Games found:" << result->size() << endl;
        return true;
    } else {
        return false;
    }
}

//********************
// RAIntegrator::getGamesNumber
//********************
int RAIntegrator::getGamesNumber(string playlist) {
    PsGames gamesList;
    getGames(&gamesList, playlist);
    return gamesList.size();

}

//********************
// RAIntegrator::getPlaylists
//********************
vector<string> RAIntegrator::getPlaylists() {
    vector<string> result;
    if (!DirEntry::exists(Env::getPathToRetroarchDir())) {
        return result;
    }
    cout << "Playlists: RA folder Found" << endl;
    string path = Env::getPathToRetroarchDir() + sep + "playlists";
    cout << "Checking path" << path << endl;
    vector<DirEntry> entries = DirEntry::diru_FilesOnly(path);
    cout << "Total Playlists:" << entries.size() << endl;
    for (const DirEntry &entry:entries) {
        if (DirEntry::getFileNameWithoutExtension(entry.name) == "AutoBleem") continue;
        if (isValidPlaylist(path + sep + entry.name)) {
            if (getGamesNumber(entry.name) > 0) {
                result.push_back(entry.name);
            }
        }
    }
    return result;
}
#endif

//********************
// RAIntegrator::autoDetectCorePath
//********************
bool RAIntegrator::autoDetectCorePath(PsGamePtr game, string &core_name, string &core_path) {
    if (findOverrideCore(game, core_name, core_path)) {
        return true;
    }
    string dbName = DirEntry::getFileNameWithoutExtension(game->db_name);
    map<string, CoreInfoPtr>::const_iterator pos = defaultCores.find(dbName);
    if (pos == defaultCores.end()) {
        core_name = "DETECT";
        core_path = "DETECT";
        return false;
    }
    core_name = pos->second->name;
    core_path = pos->second->core_path;
    return true;
}

//********************
// RAIntegrator::initCoreInfo
//********************
void RAIntegrator::initCoreInfo() {
    cout << "Building core list" << endl;
    if (!DirEntry::exists(Env::getPathToRetroarchDir())) {
        cout << "Retroarch Not Found" << endl;
        return;
    }
    cores.clear();
    databases.clear();
    defaultCores.clear();
    string infoFolder = Env::getPathToRetroarchDir() + sep + "info/";
    cout << "Scanning: " << infoFolder << endl;
    vector<DirEntry> entries = DirEntry::diru_FilesOnly(infoFolder);
    cout << "Found files:" << entries.size() << endl;
    for (const DirEntry &entry:entries) {
        if (DirEntry::getFileExtension(entry.name) == "info") {
            string fullPath = infoFolder + sep + entry.name;

            CoreInfoPtr ci = parseCoreInfo(fullPath, entry.name);
            cores.push_back(ci);
        }
    }
    sort(cores.begin(), cores.end(), sortByMaxExtensions); // why not

    for (const string &dbname:databases) {
        bool nextDb = false;

        for (CoreInfoPtr ciPtr:cores) {
            for (const string &db:ciPtr->databases) {
                if (dbname == db) {
                    defaultCores.insert(std::pair<string, CoreInfoPtr>(db, ciPtr));
                    nextDb = true;
                }
                if (nextDb) continue;
            }
            if (nextDb) continue;
        }

        map<string, CoreInfoPtr>::const_iterator pos = defaultCores.find(dbname);

        if (pos == defaultCores.end()) {

            continue;
        }

        cout << "Mapping DB: " << dbname << "  Core: " << pos->second->name << endl;
    }

    overrideCores.clear();
    ifstream in(Env::getWorkingPath() + sep + "coreOverride.cfg");
    string line;
    while (getline(in, line)) {
        string db_name = line.substr(0, line.find("="));
        string value = line.substr(line.find("=") + 1);
        cout << "Custom Core Override: " << db_name << "    core: " << value << endl;

        for (CoreInfoPtr ciPtr:cores) {
            if (ciPtr->name.find(value) != string::npos) {
                lcase(db_name);
                trim(db_name);
                overrideCores.insert(std::pair<string, CoreInfoPtr>(db_name, ciPtr));
                cout << "Found: " << db_name << "    core: " << ciPtr->name << " " << ciPtr->core_path << endl;
            }
        }

    }
    in.close();

}

//********************
// RAIntegrator::escapeName
//********************
string RAIntegrator::escapeName(string text) {
    return DirEntry::replaceTheseCharsWithThisChar(text, "&*/:`<>?\\|", '_');
}

//********************
// RAIntegrator::isGameValid
//********************
bool RAIntegrator::isGameValid(PsGamePtr game) {
    if (!DirEntry::exists(game->core_path)) {
        return false;
    }
    string path = game->image_path;
    if (path.find("#") != string::npos) {
        int pos = path.find("#");
        string check = path.substr(0, pos);
        if (!DirEntry::exists(check)) {

            return false;
        }
    } else {
        if (!DirEntry::exists(path)) {

            return false;
        }
    }
    return true;
}

//********************
// RAIntegrator::parseCoreInfo
//********************
CoreInfoPtr RAIntegrator::parseCoreInfo(string file, string entry) {
    ifstream in(file);
    string line;

    cout << "Parsing " << endl;
    CoreInfoPtr coreInfoPtr{new CoreInfo};
    coreInfoPtr->core_path = Env::getPathToRetroarchDir() + sep + "cores/" + DirEntry::getFileNameWithoutExtension(entry) + ".so";
    coreInfoPtr->extensions.clear();
    cout << "CorePath: " << coreInfoPtr->core_path << endl;
    while (getline(in, line)) {
        string lcaseline = line;
        lcase(lcaseline);

        if (lcaseline.rfind("display_name", 0) == 0) {
            string value = line.substr(lcaseline.find("=") + 1);
            value.erase(remove(value.begin(), value.end(), '\"'), value.end());
            trim(value);
            coreInfoPtr->name = value;
            cout << "CoreName: " << coreInfoPtr->name << endl;
        }
        if (lcaseline.rfind("supported_extensions", 0) == 0) {
            string value = line.substr(lcaseline.find("=") + 1);
            value.erase(remove(value.begin(), value.end(), '\"'), value.end());
            trim(value);

            coreInfoPtr->extensions.clear();
            std::stringstream check1(value);

            string intermediate;

            while (getline(check1, intermediate, '|')) {
                coreInfoPtr->extensions.push_back(intermediate);

            }
        }
        if (lcaseline.rfind("database", 0) == 0) {
            string value = line.substr(lcaseline.find("=") + 1);
            value.erase(remove(value.begin(), value.end(), '\"'), value.end());
            trim(value);

            coreInfoPtr->databases.clear();
            std::stringstream check1(value);

            string intermediate;

            while (getline(check1, intermediate, '|')) {
                coreInfoPtr->databases.push_back(intermediate);
                databases.insert(intermediate);
            }
        }

    }
    in.close();
    return coreInfoPtr;
}