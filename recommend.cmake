function(recommend_monotonicity BINS R S)
    if (BINS EQUAL 3 AND R EQUAL 45 AND S EQUAL 33)
        set(IMONOT 5)
    endif ()

    # 3 bins:
    if ((BINS EQUAL 3) AND (R EQUAL 45) AND (S EQUAL 33))
        set(IMONOT 5)
    elseif ((BINS EQUAL 3) AND (R EQUAL 72) AND (S EQUAL 53))
        set(IMONOT 6)
    elseif ((BINS EQUAL 3) AND (R EQUAL 86) AND (S EQUAL 63))
        set(IMONOT 6)
    elseif ((BINS EQUAL 3) AND (R EQUAL 112) AND (S EQUAL 82))
        set(IMONOT 8)
    elseif ((BINS EQUAL 3) AND (R EQUAL 123) AND (S EQUAL 90))
        math(EXPR IMONOT "${S}-1")
    elseif ((BINS EQUAL 3) AND (R EQUAL 138) AND (S EQUAL 101))
        set(IMONOT 20)
    elseif ((BINS EQUAL 3) AND (R EQUAL 153) AND (S EQUAL 112))
        set(IMONOT 8)
    elseif ((BINS EQUAL 3) AND (R EQUAL 164) AND (S EQUAL 120))
        set(IMONOT 15)
    elseif ((BINS EQUAL 3) AND (R EQUAL 175) AND (S EQUAL 128))
        set(IMONOT 25)
    elseif ((BINS EQUAL 3) AND (R EQUAL 190) AND (S EQUAL 139))
        set(IMONOT 25)
    elseif ((BINS EQUAL 3) AND (R EQUAL 194) AND (S EQUAL 142))
        set(IMONOT 15)
    elseif ((BINS EQUAL 3) AND (R EQUAL 205) AND (S EQUAL 150))
        set(IMONOT 15)
    elseif ((BINS EQUAL 3) AND (R EQUAL 329) AND (S EQUAL 240)) # For some specific configurations.
        set(IMONOT 35)
    elseif ((BINS EQUAL 3) AND (R EQUAL 411) AND (S EQUAL 300))
        set(IMONOT 15)
        set(ISCALE 6)
    elseif ((BINS EQUAL 3) AND (R EQUAL 657) AND (S EQUAL 480))
        set(IMONOT 60)
    elseif ((BINS EQUAL 3) AND (R EQUAL 821) AND (S EQUAL 600))
        set(IMONOT 70)
    elseif ((BINS EQUAL 3) AND (R EQUAL 985) AND (S EQUAL 720))
        set(IMONOT 60)
    elseif ((BINS EQUAL 3)) # Default on 3 bins.
        math(EXPR IMONOT "${S}-1")
        # 4 bins:
    elseif ((BINS EQUAL 4) AND (R EQUAL 19) AND (S EQUAL 14))
        set(IMONOT 2)
    elseif ((BINS EQUAL 4) AND (R EQUAL 72) AND (S EQUAL 53))
        set(IMONOT 6)
        # elseif ((BINS EQUAL 4) AND (R EQUAL 60) AND (S EQUAL 44))
        # set (IMONOT 43)
    elseif ((BINS EQUAL 4) AND (R EQUAL 112) AND (S EQUAL 82))
        set(IMONOT 16)
    elseif ((BINS GREATER_EQUAL 8) AND (IBINS LESS_EQUAL 9) AND (R EQUAL 19) AND (S EQUAL 14))
        set(IMONOT 1)
    elseif ((BINS GREATER_EQUAL 10) AND (R EQUAL 19) AND (S EQUAL 14))
        set(IMONOT 1)
    elseif ((BINS EQUAL 6) AND (R EQUAL 15) AND (S EQUAL 11))
        set(IMONOT 3)
    elseif ((BINS GREATER_EQUAL 7) AND (R EQUAL 15) AND (S EQUAL 11))
        set(IMONOT 5)
    else ()
        math(EXPR IMONOT "${S} - 1")
    endif ()
    return(PROPAGATE IMONOT)
endfunction()

function(recommend_scaling BINS R S)
    if ((BINS EQUAL 3) AND (R EQUAL 112) AND (S EQUAL 82))
        set(ISCALE 12)
    elseif ((BINS EQUAL 3) AND (R EQUAL 329) AND (S EQUAL 240)) # For some specific configurations.
        set(ISCALE 6)
    elseif ((BINS EQUAL 3) AND (R EQUAL 411) AND (S EQUAL 300))
        set(ISCALE 6)
    elseif ((BINS EQUAL 3) AND (R EQUAL 657) AND (S EQUAL 480))
        set(ISCALE 6)
    elseif ((BINS EQUAL 3) AND (R EQUAL 821) AND (S EQUAL 600))
        set(ISCALE 6)
    elseif ((BINS EQUAL 3) AND (R EQUAL 985) AND (S EQUAL 720))
        set(ISCALE 6)
    elseif ((BINS EQUAL 3)) # Default on 3 bins.
        set(ISCALE 6)
        # 4 bins:
    elseif ((BINS EQUAL 4) AND (R EQUAL 19) AND (S EQUAL 14))
        set(ISCALE 6)
    elseif ((BINS GREATER_EQUAL 8) AND (IBINS LESS_EQUAL 9) AND (R EQUAL 19) AND (S EQUAL 14))
        set(ISCALE 6)
    elseif ((BINS GREATER_EQUAL 10) AND (R EQUAL 19) AND (S EQUAL 14))
        set(ISCALE 3)
    else ()
        set(ISCALE 3)
    endif ()
    return(PROPAGATE ISCALE)
endfunction()