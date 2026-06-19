//
//  StringUtils.swift
//  macos-ui
//
//  Created by Michele Verriello on 19/06/2026.
//

import Foundation

func convertCStringToSwiftString(_ cString: inout UnsafeMutablePointer<CChar>?) -> String? {
    guard let pointer = cString else {
        return nil
    }

    defer {
        free(pointer)
        cString = nil
    }

    return String(cString: pointer)
}
