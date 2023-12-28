log_string()
{
    # $1 -> the json error file name
    # $2 -> string to be appended
    head -q -n -2 $1 > /tmp/out.json
    head -q -c -1  /tmp/out.json > $1
    echo -e -n $2 >> $1
    echo -e "\n\t]\n}" >> $1
    rm -rf /tmp/out.json
}

# This function will log the error to the output JSON file
# The format of JSON file is
#
# {
#    "Errors":
#    [
#        {
#            "error_name":"err1",
#            "error_params":{"l":"m","p":"q","x":"z"},
#            "default_message": "error1 occured"
#        },
#        {
#            "error_name":"err2",
#            "error_params":{"l":"m","p":"q","x":"z"},
#            "default_message": "error2 occured"
#        },
#        {
#            "error_name":"err3",
#            "error_params":{"l":"m","p":"q","x":"z"},
#            "default_message": "error3 occured"
#        }
#    ]
# }

log_to_json_errors_file()
{
    local json_errors_file=$1
    error_code=$2
    error_message=$3
    nr_args=$#

    if [ ! -f $json_errors_file ]; then
        echo -e "{\n\t\"Errors\":\n\t[\n\t]\n}" >> $json_errors_file
    else
        log_string $json_errors_file ","
    fi
    log_string  $json_errors_file "\n\t\t{"
    log_string  $json_errors_file "\n\t\t\t\"error_name\":\"${error_code}\","
    
    nr_args=`expr ${nr_args} - 3`
    shift; shift; shift
    nr_params=0
    str=""
    while [ ${nr_args} -gt "0" ]
    do
        if [ ${nr_params} -gt "0" ]; then
            str="$str,"
        fi
        str="$str\"$1\":\""
        shift
        str="$str$1\""
        shift
        nr_params=`expr ${nr_params} + 1`
        nr_args=`expr ${nr_args} - 2`
    done

    str="\n\t\t\t\"error_params\":{$str},"
    log_string $json_errors_file $str

    log_string $json_errors_file "\n\t\t\t\"default_message\":\"${error_message}\"\n\t\t}"

}

SYSTEMCTL_DISTRO="SLES15-64 OL8-64 OL9-64 UBUNTU-20.04-64 UBUNTU-22.04-64 DEBIAN9-64 DEBIAN10-64 DEBIAN11-64 RHEL9-64"

#FUNCTION : check if list contains the element, 0->contains, 1->doesn't contain
list_contains()
{
    [[ $1 =~ (^|[[:space:]])$2($|[[:space:]]) ]] && return "0" || return "1"
}
