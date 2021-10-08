# first argument is the API token
# second argument is the job id

curl -s -X POST \
    -H "Content-Type: application/json" \
    -H "Accept: application/json" \
    -H "Travis-API-Version: 3" \
    -H "Authorization: token $1" \
    -d '{ "quiet": true }' \
    https://api.travis-ci.com/job/$2/debug