string User::to_json() const {
    json j;
    j["email"] = this->email;
    j["username"] = this->username;
    j["password"] = this->password;
    return j.dump();
}
