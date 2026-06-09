#pragma once

class IMesh {
public:
    virtual ~IMesh() = default;
    virtual void draw() const = 0;
};