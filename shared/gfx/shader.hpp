#pragma once

class Shader {
public:
    virtual ~Shader() = default;
    virtual bool bind() = 0;
};