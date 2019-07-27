//
// Created by screemer on 2019-07-25.
//

#include "ra_integrator.h"
#include "fstream"
#include <cstdio>
#include <iostream>
#include <sstream>
#include "../util.h"
#include "../nlohmann/json.h"
#include "../engine/cfgprocessor.h"
#include "../DirEntry.h"

using namespace nlohmann;

bool RAIntegrator::isValidPlaylist(string path) {
    // check file extension
    if (ReturnLowerCase(DirEntry::getFileExtension(path)) != "lpl") {
        return false;
    }
    // check if not empty
    std::ifstream in(path, std::ifstream::ate | std::ifstream::binary);
    if (in.tellg() <= 0) {
        return false;
    }

    return true;
}

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

void RAIntegrator::parseJSON(PsGames *result, string path) {
    std::ifstream in(path, std::ifstream::binary);
    json j;
    in >> j;

    int id = 0;

    json array = j["items"];

    for (json::iterator it = array.begin(); it != array.end(); ++it) {
        PsGamePtr game{new PsGame};
        game->gameId = id++;
        game->title = (*it)["label"];
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
        game->core_name = (*it)["core_name"];
        game->core_path = (*it)["core_path"];
        game->db_name = (*it)["db_name"];
        game->image_path = (*it)["path"];

        if ((game->core_path == "DETECT") || (game->core_name == "DETECT")) {
            autoDetectCorePath(game, game->core_name, game->core_path);
        }

        if (!DirEntry::exists(game->core_path)) {
            autoDetectCorePath(game, game->core_name, game->core_path);
        }
        if (isGameValid(game)) {
            result->push_back(game);
        }
    }
    in.close();


}

void RAIntegrator::parse6line(PsGames *result, string path) {
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
        if (in.eof()) return;
        getline(in, label);
        if (in.eof()) return;
        getline(in, core_path);
        if (in.eof()) return;
        getline(in, core_name);
        if (in.eof()) return;
        getline(in, crc);
        if (in.eof()) return;
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
            autoDetectCorePath(game, core_name, core_path);

            game->core_name = core_name;
            game->core_path = core_path;
        }
        if (!DirEntry::exists(game->core_path)) {
            autoDetectCorePath(game, core_name, core_path);

            game->core_name = core_name;
            game->core_path = core_path;

        }
        if (isGameValid(game)) {
            result->push_back(game);
        }
    }
    in.close();
}

bool RAIntegrator::getGames(PsGames *result, string playlist) {
    string path = string(RA_FOLDER) + DirEntry::separator() + "playlists" + DirEntry::separator() + playlist;
    if (isJSONPlaylist(path)) {
        parseJSON(result, path);
    } else {
        parse6line(result, path);
    }
    return true;
}

vector<string> RAIntegrator::getPlaylists() {
    vector<string> result;
    if (!DirEntry::exists(RA_FOLDER)) {
        return result;
    }

    string path = string(RA_FOLDER) + DirEntry::separator() + "playlists";
    vector<DirEntry> entries = DirEntry::diru_FilesOnly(path);
    for (const DirEntry &entry:entries) {
        if (DirEntry::getFileNameWithoutExtension(entry.name) == "AutoBleem") continue;
        if (isValidPlaylist(path + DirEntry::separator() + entry.name)) {
            result.push_back(entry.name);
        }
    }
    return result;
}

void RAIntegrator::autoDetectCorePath(PsGamePtr game, string &core_name, string &core_path) {
    //TODO: use extension as well to find core
    string dbName = DirEntry::getFileNameWithoutExtension(game->db_name);
    map<string, CoreInfoPtr>::const_iterator pos = defaultCores.find(dbName);
    if (pos == defaultCores.end()) {
        core_name = "???";
        core_path = "???";
        return;
    }
    core_name = pos->second->name;
    core_path = pos->second->core_path;
}

void RAIntegrator::initCoreInfo() {
    cout << "Building core list" << endl;
    if (!DirEntry::exists(RA_FOLDER)){
        cout << "Retroarch Not Found" << endl;
        return;
    }
    cores.clear();
    databases.clear();
    defaultCores.clear();
    string infoFolder = string(RA_FOLDER) + DirEntry::separator() + "info/";
    cout << "Scanning: " << infoFolder << endl;
    vector<DirEntry> entries = DirEntry::diru_FilesOnly(infoFolder);
    cout << "Found files:" << entries.size() << endl;
    for (const DirEntry &entry:entries) {
        cout << "Checking file: " << entry.name << endl;
        if (DirEntry::getFileExtension(entry.name) == "info") {
            string fullPath = infoFolder +  entry.name;
            cout << "Reading info :" <<fullPath << endl;
            CoreInfoPtr ci = parseInfo(fullPath, entry.name);
            cores.push_back(ci);
        } else
        {
            cout << "Incorrect extension" << endl;
        }
    }
    sort(cores.begin(), cores.end(), sortByMaxExtensions); // why not

    for (const string &dbname:databases) {
        bool nextDb = false;

        for (CoreInfoPtr ciPtr:cores) {
            for (const string &db:ciPtr->databases) {
                if (dbname == db) {
                    // detected core for db
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
}

string RAIntegrator::escapeName(string text) {
    // &*/:`<>?\|
    for (std::string::iterator it = text.begin(); it != text.end(); ++it) {
        if (*it == '&') {
            *it = '_';
        }
        if (*it == '*') {
            *it = '_';
        }
        if (*it == '/') {
            *it = '_';
        }
        if (*it == ':') {
            *it = '_';
        }
        if (*it == '`') {
            *it = '_';
        }
        if (*it == '<') {
            *it = '_';
        }
        if (*it == '>') {
            *it = '_';
        }
        if (*it == '?') {
            *it = '_';
        }
        if (*it == '\\') {
            *it = '_';
        }
        if (*it == '|') {
            *it = '_';
        }
    }
    return text;
}

bool RAIntegrator::isGameValid(PsGamePtr game)
{
    if (!DirEntry::exists(game->core_path) )
    {
        return false;
    }

    string path = game->image_path;

    if (path.find("#") != string::npos)
    {
        int pos = path.find("#");
        string check = path.substr(0,pos);
        cout << check << endl;
        if (!DirEntry::exists(check) )
        {
            return false;
        }
    } else
    {
        if (!DirEntry::exists(path) )
        {
            return false;
        }
    }
    return true;
}

CoreInfoPtr RAIntegrator::parseInfo(string file, string entry) {
    ifstream in(file);
    string line;


    cout << endl;
    CoreInfoPtr coreInfoPtr{new CoreInfo};
    coreInfoPtr->core_path = string(RA_FOLDER) + "/cores/" + DirEntry::getFileNameWithoutExtension(entry) + ".so";
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